#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <map>
#include <memory>
#include <fstream>
#include <chrono>
#include <functional>
#include "DataPreprocessor.h"
#include "MerkleTree.h"
#include "IntegrityVerifier.h"
#include "ExistenceProver.h"
#include "TamperingDetector.h"
#include "PerformanceMeasurer.h"

using namespace std;

//useless function for gtest
int doubler(int a)
{
    return a * 2;
}

class CLI
{
private:
    DataPreprocessor dataProcessor;
    MerkleTree merkleTree;
    IntegrityVerifier integrityVerifier;
    unique_ptr<ExistenceProof> existenceProver;
    unique_ptr<TamperDetector> tamperDetector;
    PerformanceMeasurer performanceMeasurer;

    vector<string> reviewData;
    vector<string> reviewIds;
    vector<Review> currentReviews;
    bool treeBuilt = false;
    string currentDataset;
    //metrics recording
    string metricsFilename = "performance_metrics.txt";
    ofstream metricsFile;

public:
    CLI() : existenceProver(nullptr), tamperDetector(nullptr) {
        initializeMetricsFile();
    }

    ~CLI()
    {
        if (metricsFile.is_open())
        {
            metricsFile.close();
        }
    }

    void run()
    {
        cout << "@~* Merkle Tree-based Cryptographic Verification for Amazon Reviews *~@" << endl;

        while (true)
        {
            displayMainMenu();
            int choice = getMenuChoice();

            if (choice == 0)
            {
                cout << "Exiting system. Goodbye!" << endl;
                break;
            }

            handleMenuChoice(choice);
        }
    }

private:
    void displayMainMenu()
    {
        cout << "\nMain Menu" << endl;
        cout << "1. Load Dataset" << endl;
        cout << "2. Display Dataset (Tabular)" << endl;
        cout << "3. Build Merkle Tree" << endl;
        cout << "4. Integrity Verification" << endl;
        cout << "5. Existence Proofs" << endl;
        cout << "6. Tamper Detection" << endl;
        cout << "7. Performance Tests" << endl;
        cout << "8. Run All Test Cases" << endl;
        cout << "9. Run Specific Test Case" << endl;
        cout << "10. Add Single Review (Partial Rebuild)" << endl;
        cout << "11. Print Merkle Tree Structure" << endl;
        cout << "12. Export Metrics" << endl;
        cout << "0. Exit" << endl;
        cout << "Enter your choice: ";
    }

    int getMenuChoice()
    {
        int choice;
        cin >> choice;
        cin.ignore();
        return choice;
    }

    void handleMenuChoice(int choice)
    {
        switch (choice)
        {
        case 1: loadDataset(); break;
        case 2: displayDatasetTabular(); break;
        case 3: buildMerkleTree(); break;
        case 4: integrityVerificationMenu(); break;
        case 5: existenceProofsMenu(); break;
        case 6: tamperDetectionMenu(); break;
        case 7: performanceTestsMenu(); break;
        case 8: runAllTestCases(); break;
        case 9: runSpecificTestCase(); break;
        case 10: addSingleReview(); break;
        case 11: printMerkleTree(); break;
        case 12:  exportMetrics(); break;
        default: cout << "Invalid choice. Please try again." << endl;
        }
    }

    void loadDataset()
    {
        cout << "\nLoad Dataset" << endl;
        cout << "Available datasets:" << endl;
        cout << "1. data/Electronics_5.json (6,739,590 reviews)" << endl;
        cout << "2. data/Automotive_5.json (1,711,519 reviews)" << endl;
        cout << "3. data/Toys_and_Games_5.json (1,828,971 reviews)" << endl;
        cout << "4. Custom file path" << endl;
        cout << "Enter choice: ";

        int datasetChoice;
        cin >> datasetChoice;
        cin.ignore();

        string filename;
        switch (datasetChoice)
        {
        case 1: filename = "data/Electronics_5.json"; break;
        case 2: filename = "data/Automotive_5.json"; break;
        case 3: filename = "data/Toys_and_Games_5.json"; break;
        case 4:
            cout << "Enter full file path: ";
            getline(cin, filename);
            break;
        default:
            cout << "Invalid choice." << endl;
            return;
        }

        cout << "Enter number of records to load (0 for all): ";
        int maxRecords;
        cin >> maxRecords;
        cin.ignore();

        cout << "Loading dataset: " << filename << endl;
        auto loadStart = chrono::high_resolution_clock::now();

        if (dataProcessor.loadFromJSON(filename, maxRecords))
        {
            auto loadEnd = chrono::high_resolution_clock::now();
            auto loadDuration = chrono::duration_cast<chrono::milliseconds>(loadEnd - loadStart);

            currentReviews = dataProcessor.getReviews();
            currentDataset = filename;
            prepareDataForTree();

            //record
            recordMetric("Dataset Load Time", to_string(loadDuration.count()), "ms");
            recordMetric("Dataset Size", to_string(currentReviews.size()), "records");
            recordMetric("Dataset File", filename);

            cout << "Dataset loaded successfully. " << currentReviews.size() << " reviews loaded." << endl;
        }
        else
        {
            cout << "Failed to load dataset. Please check file path." << endl;
        }
    }

    void displayDatasetTabular()
    {
        if (currentReviews.empty())
        {
            cout << "No dataset loaded." << endl;
            return;
        }

        cout << "\nDataset Table" << endl;
        cout << "Total Reviews: " << currentReviews.size() << endl;
        cout << "Dataset: " << currentDataset << endl;
        cout << endl;

        // table header
        cout << left << setw(25) << "Review ID"
            << setw(15) << "Product ID"
            << setw(15) << "Reviewer ID"
            << setw(8) << "Rating"
            << setw(40) << "Summary" << endl;
        cout << string(103, '-') << endl;

        // display first 10 reviews in tabular format
        for (int i = 0; i < min(10, (int)currentReviews.size()); i++)
        {
            const Review& r = currentReviews[i];
            string summary = r.summary.length() > 35 ? r.summary.substr(0, 35) + "..." : r.summary;

            cout << left << setw(25) << r.getUniqueID().substr(0, 24)
                << setw(15) << r.asin.substr(0, 14)
                << setw(15) << r.reviewerID.substr(0, 14)
                << setw(8) << r.overall
                << setw(40) << summary << endl;
        }

        if (currentReviews.size() > 10)
        {
            cout << "... and " << (currentReviews.size() - 10) << " more reviews" << endl;
        }
    }

    void prepareDataForTree()
    {
        reviewData.clear();
        reviewIds.clear();

        for (const auto& review : currentReviews)
        {
            reviewData.push_back(review.convertToString());
            reviewIds.push_back(review.getUniqueID());
        }
    }

    void buildMerkleTree()
    {
        if (currentReviews.empty())
        {
            cout << "Please load a dataset first." << endl;
            return;
        }

        cout << "\nBuilding Merkle Tree" << endl;

        auto start = chrono::high_resolution_clock::now();
        merkleTree.buildTreeFromReviews(reviewData, reviewIds);
        auto end = chrono::high_resolution_clock::now();

        auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);

        //record build metrics
        recordMetric("Total Build Time", to_string(duration.count()), "ms");
        recordMetric("Leaf Count", to_string(merkleTree.getLeafCount()));
        recordMetric("Merkle Root", merkleTree.getRootHash().substr(0, 32) + "...");

        //calculate and record average hash time
        double avgHashTime = duration.count() / (double)merkleTree.getLeafCount();
        recordMetric("Hash Time (avg)", to_string(avgHashTime), "ms per record");

        cout << "Tree construction completed in " << duration.count() << " ms" << endl;
        cout << "Merkle Root: " << merkleTree.getRootHash() << endl;
        cout << "Leaf Count: " << merkleTree.getLeafCount() << endl;

        treeBuilt = true;

        // initialize supporting modules
        existenceProver = make_unique<ExistenceProof>(&merkleTree);
        existenceProver->indexReviews(currentReviews);

        tamperDetector = make_unique<TamperDetector>(&merkleTree, currentReviews);
        tamperDetector->setDatasetName(currentDataset);

        integrityVerifier.storeRootHash(currentDataset, merkleTree.getRootHash());
    }

    void integrityVerificationMenu()
    {
        if (!treeBuilt)
        {
            cout << "Please build the Merkle tree first." << endl;
            return;
        }

        cout << "\nIntegrity Verification" << endl;
        cout << "1. Save Current Root" << endl;
        cout << "2. Compare with Stored Root" << endl;
        cout << "3. List Stored Roots" << endl;
        cout << "4. Manual Root Comparison" << endl;
        cout << "Enter choice: ";

        int choice;
        cin >> choice;
        cin.ignore();

        switch (choice)
        {
        case 1:
            integrityVerifier.saveRootToFile("stored_roots.txt", currentDataset, merkleTree.getRootHash());
            cout << "Root saved successfully." << endl;
            break;
        case 2:
        {
            string result = integrityVerifier.compareWithStored(currentDataset);
            cout << result << endl;
            break;
        }
        case 3:
            integrityVerifier.listStoredRoots();
            break;
        case 4:
        {
            cout << "Enter first root hash: ";
            string root1, root2;
            getline(cin, root1);
            cout << "Enter second root hash: ";
            getline(cin, root2);
            string result = IntegrityVerifier::compareRoots(root1, root2);
            cout << result << endl;
            break;
        }
        default:
            cout << "Invalid choice." << endl;
        }
    }

    void existenceProofsMenu()
    {
        if (!treeBuilt || !existenceProver)
        {
            cout << "Please build the Merkle tree first." << endl;
            return;
        }

        cout << "\nExistence Proofs" << endl;
        cout << "1. Generate Proof for Review ID" << endl;
        cout << "2. Generate Proofs for Product" << endl;
        cout << "3. Batch Proof Generation" << endl;
        cout << "4. Benchmark Proof System" << endl;
        cout << "Enter choice: ";

        int choice;
        cin >> choice;
        cin.ignore();

        switch (choice)
        {
        case 1:
        {
            cout << "Enter Review ID: ";
            string reviewId;
            getline(cin, reviewId);
            ProofResult result = existenceProver->generateReviewProof(reviewId);
            result.print();
            break;
        }
        case 2:
        {
            cout << "Enter Product ID: ";
            string productId;
            getline(cin, productId);
            vector<ProofResult> results = existenceProver->generateProductProofs(productId);
            cout << "Generated " << results.size() << " proofs for product " << productId << endl;
            if (!results.empty()) results[0].print();
            break;
        }
        case 3:
        {
            cout << "Enter number of proofs to generate: ";
            int count;
            cin >> count;
            cin.ignore();

            vector<string> testIds;
            for (int i = 0; i < min(count, (int)reviewIds.size()); i++)
            {
                testIds.push_back(reviewIds[i]);
            }
            existenceProver->batchGenerateProofs(testIds);
            break;
        }
        case 4:
        {
            cout << "Enter sample size: ";
            int sampleSize;
            cin >> sampleSize;
            cin.ignore();
            existenceProver->benchmarkProofSystem(sampleSize);
            break;
        }
        default:
            cout << "Invalid choice." << endl;
        }
    }

    void addSingleReview()
    {
        if (!treeBuilt)
        {
            cout << "Please build the Merkle tree first." << endl;
            return;
        }

        cout << "\nAdd Single Review (Partial Rebuild)" << endl;

        string reviewerId, asin, reviewText, summary;
        double overall;

        cout << "Enter Reviewer ID: ";
        getline(cin, reviewerId);
        cout << "Enter Product ID (ASIN): ";
        getline(cin, asin);
        cout << "Enter Review Text: ";
        getline(cin, reviewText);
        cout << "Enter Summary: ";
        getline(cin, summary);
        cout << "Enter Rating (1.0-5.0): ";
        cin >> overall;
        cin.ignore();

        Review newReview = createReview(reviewerId, asin, reviewText, summary, overall);

        currentReviews.push_back(newReview);
        reviewData.push_back(newReview.convertToString());
        reviewIds.push_back(newReview.getUniqueID());

        try {
            string oldRoot = merkleTree.getRootHash();
            merkleTree.addReview(newReview.convertToString(), newReview.getUniqueID());
            string newRoot = merkleTree.getRootHash();

            cout << "Review added successfully." << endl;
            cout << "Old root: " << oldRoot.substr(0, 32) << "..." << endl;
            cout << "New root: " << newRoot.substr(0, 32) << "..." << endl;

            existenceProver->indexReviews(currentReviews);
            tamperDetector->setDatasetName(currentDataset);
            integrityVerifier.storeRootHash(currentDataset, newRoot);

        }
        catch (const exception& e)
        {
            cout << "Error adding review: " << e.what() << endl;
        }
    }

    Review createReview(const string& reviewerId, const string& asin,
        const string& reviewText, const string& summary, double overall)
    {
        Review review = currentReviews[0];

        review.reviewerID = reviewerId;
        review.asin = asin;
        review.reviewText = reviewText;
        review.summary = summary;
        review.overall = overall;
        review.unixReviewTime = to_string(time(nullptr));
        review.reviewID = reviewerId + "_" + asin + "_" + review.unixReviewTime;

        return review;
    }

    void tamperDetectionMenu()
    {
        if (!treeBuilt || !tamperDetector)
        {
            cout << "Please build the Merkle tree first." << endl;
            return;
        }

        cout << "\nTamper Detection" << endl;
        cout << "1. Simulate Single Modification" << endl;
        cout << "2. Simulate Multiple Deletions" << endl;
        cout << "3. Simulate Review Injection" << endl;
        cout << "4. Simulate Rating Manipulation" << endl;
        cout << "5. Comprehensive Tamper Analysis" << endl;
        cout << "Enter choice: ";

        int choice;
        cin >> choice;
        cin.ignore();

        vector<Review> tamperedReviews;
        MerkleTree testTree;

        switch (choice)
        {
        case 1:
            tamperedReviews = tamperDetector->tamperWithReviews(currentReviews, 1);
            break;
        case 2:
            tamperedReviews = tamperDetector->deleteReviews(currentReviews, 3);
            break;
        case 3:
            tamperedReviews = tamperDetector->injectReviews(currentReviews, 2);
            break;
        case 4:
            tamperedReviews = tamperDetector->manipulateRatings(currentReviews, 2);
            break;
        case 5:
        {
            tamperedReviews = tamperDetector->injectReviews(currentReviews, 2);
            vector<string> testData, testIds;
            for (const auto& review : tamperedReviews)
            {
                testData.push_back(review.convertToString());
                testIds.push_back(review.getUniqueID());
            }
            testTree.buildTreeFromReviews(testData, testIds);
            ComprehensiveTamperReport report = tamperDetector->comprehensiveAnalysis(tamperedReviews, testTree);
            report.print();
            return;
        }
        default:
            cout << "Invalid choice." << endl;
            return;
        }

        if (choice >= 1 && choice <= 4)
        {
            vector<string> testData, testIds;
            for (const auto& review : tamperedReviews)
            {
                testData.push_back(review.convertToString());
                testIds.push_back(review.getUniqueID());
            }
            testTree.buildTreeFromReviews(testData, testIds);
            TamperResult result = tamperDetector->detectByRootComparison(testTree.getRootHash());
            result.print();
        }
    }

    void performanceTestsMenu()
    {
        if (currentReviews.empty())
        {
            cout << "Please load a dataset first." << endl;
            return;
        }

        cout << "\nPerformance Tests" << endl;
        cout << "1. Comprehensive Performance Analysis" << endl;
        cout << "2. Scalability Test" << endl;
        cout << "3. Proof Generation Benchmark (1000 proofs)" << endl;
        cout << "4. Memory Usage Analysis" << endl;
        cout << "5. Hash Performance Test" << endl;
        cout << "Enter choice: ";

        int choice;
        cin >> choice;
        cin.ignore();

        switch (choice)
        {
        case 1:
            performanceMeasurer.runComprehensiveAnalysis(currentReviews);
            break;
        case 2:
        {
            vector<int> sizes = { 100, 500, 1000, 5000 };
            auto results = performanceMeasurer.measureScalability(currentReviews, sizes);
            cout << "\nScalability Results" << endl;
            for (const auto& result : results)
            {
                result.print();
                recordMetric("Scalability Test - " + to_string(result.datasetSize) + " records",
                    to_string(result.executionTimeMicroseconds/ 1000), "ms");
            }
            break;
        }
        case 3:
        {
            if (treeBuilt && existenceProver)
            {
                cout << "Benchmarking 1000 proof generations..." << endl;
                auto start = chrono::high_resolution_clock::now();
                existenceProver->benchmarkProofSystem(1000);
                auto end = chrono::high_resolution_clock::now();
                auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);

                recordMetric("Proof Generation Time (1000 proofs)", to_string(duration.count()), "ms");
                recordMetric("Proof Generation Time (avg)", to_string(duration.count() / 1000.0), "ms per proof");
            }
            else
            {
                cout << "Please build Merkle tree first." << endl;
            }
            break;
        }
        case 4:
        {
            cout << "\nMemory Usage Analysis" << endl;
            cout << "Review object size: " << sizeof(Review) << " bytes" << endl;
            cout << "MerkleNode size: " << sizeof(MerkleNode) << " bytes" << endl;
            cout << "Total reviews: " << currentReviews.size() << endl;

            size_t reviewMemory = currentReviews.size() * sizeof(Review);
            size_t treeMemory = currentReviews.size() * 2 * sizeof(MerkleNode);
            size_t totalMemory = reviewMemory + treeMemory;

            cout << "Estimated minimum memory: " << (reviewMemory / 1024) << " KB" << endl;
            cout << "Tree nodes estimate: " << (currentReviews.size() * 2) << " nodes" << endl;
            cout << "Estimated tree memory: " << (treeMemory / 1024) << " KB" << endl;
            cout << "Total estimated memory: " << (totalMemory / 1024) << " KB" << endl;

            recordMetric("Memory Usage - Review Objects", to_string(reviewMemory / 1024), "KB");
            recordMetric("Memory Usage - Tree Structure", to_string(treeMemory / 1024), "KB");
            recordMetric("Memory Usage - Total Estimated", to_string(totalMemory / 1024), "KB");
            break;
        }
        case 5:
        {
            runHashPerformanceTest();
            break;
        }
        default:
            cout << "Invalid choice." << endl;
        }
    }

    void printMerkleTree()
    {
        if (!treeBuilt)
        {
            cout << "Please build the Merkle tree first." << endl;
            return;
        }
        cout << "Enter number of levels to print (max 100): ";
        int levels;
        do
        {
            cin >> levels;
            if (levels <= 0 || levels > 100)
                cout << "Invalid choice. Try again: ";
        } while (levels < 0 || levels > 100);
        merkleTree.printTree(levels);
    }

    void runAllTestCases()
    {
        cout << "\nRunning All Test Cases" << endl;

        // test case 1: load dataset and build tree
        cout << "\n1. Loading dataset and building Merkle tree..." << endl;
        if (currentReviews.empty())
        {
            loadDataset();
        }
        if (!treeBuilt)
        {
            buildMerkleTree();
        }
        cout << "Merkle tree built with root: " << merkleTree.getRootHash().substr(0, 32) << "..." << endl;

        // test case 2: save merkle root
        cout << "\n2. Saving Merkle root..." << endl;
        integrityVerifier.saveRootToFile("stored_roots.txt", currentDataset, merkleTree.getRootHash());
        cout << "Root saved successfully" << endl;

        // test case 3: query existing review
        cout << "\n3. Querying existing review..." << endl;
        if (!reviewIds.empty())
        {
            ProofResult proof = existenceProver->generateReviewProof(reviewIds[0]);
            cout << "Review exists: " << proof.reviewId << endl;
            cout << "Proof size: " << proof.proofPath.size() << " elements" << endl;
            cout << "Verified: " << (proof.verified ? "YES" : "NO") << endl;
        }

        // test case 4: query non-existing review
        cout << "\n4. Querying non-existing review..." << endl;
        ProofResult nonExistProof = existenceProver->generateReviewProof("NON_EXISTENT_REVIEW_12345");
        cout << nonExistProof.status << endl;

        // test case 5: modify review text
        cout << "\n5. Testing single review modification..." << endl;
        vector<Review> modified = tamperDetector->tamperWithReviews(currentReviews, 1);
        vector<string> modData, modIds;
        for (const auto& review : modified)
        {
            modData.push_back(review.convertToString());
            modIds.push_back(review.getUniqueID());
        }
        MerkleTree modTree;
        modTree.buildTreeFromReviews(modData, modIds);
        TamperResult modResult = tamperDetector->detectByRootComparison(modTree.getRootHash());
        cout << modResult.status << endl;

        // test case 6: single character modification
        cout << "\n6. Testing single character modification..." << endl;
        if (!currentReviews.empty())
        {
            vector<Review> charModified = currentReviews;
            charModified[0].reviewText = "modified text";
            vector<string> charData, charIds;
            for (const auto& review : charModified)
            {
                charData.push_back(review.convertToString());
                charIds.push_back(review.getUniqueID());
            }
            MerkleTree charTree;
            charTree.buildTreeFromReviews(charData, charIds);
            string result = IntegrityVerifier::compareRoots(merkleTree.getRootHash(), charTree.getRootHash());
            cout << result << endl;
        }

        // test case 7: delete review
        cout << "\n7. Testing review deletion..." << endl;
        vector<Review> deleted = tamperDetector->deleteReviews(currentReviews, 1);
        vector<string> delData, delIds;
        for (const auto& review : deleted)
        {
            delData.push_back(review.convertToString());
            delIds.push_back(review.getUniqueID());
        }
        MerkleTree delTree;
        delTree.buildTreeFromReviews(delData, delIds);
        TamperResult delResult = tamperDetector->detectByRootComparison(delTree.getRootHash());
        cout << delResult.status << endl;

        // test case 8: insert fake record
        cout << "\n8. Testing fake record insertion..." << endl;
        vector<Review> injected = tamperDetector->injectReviews(currentReviews, 1);
        vector<string> injData, injIds;
        for (const auto& review : injected)
        {
            injData.push_back(review.convertToString());
            injIds.push_back(review.getUniqueID());
        }
        MerkleTree injTree;
        injTree.buildTreeFromReviews(injData, injIds);
        TamperResult injResult = tamperDetector->detectByRootComparison(injTree.getRootHash());
        cout << injResult.status << endl;

        // test case 9: compare roots
        cout << "\n9. Comparing current vs stored root..." << endl;
        string compareResult = integrityVerifier.compareWithStored(currentDataset);
        cout << compareResult << endl;

        // test case 10: proof generation performance
        cout << "\n10. Testing proof generation performance (100 proofs)..." << endl;
        existenceProver->benchmarkProofSystem(100);

        cout << "\nAll test cases completed" << endl;
    }

    void runSpecificTestCase()
    {
        cout << "\nRun Specific Test Case" << endl;
        cout << "1. Load 1M records and build tree" << endl;
        cout << "2. Proof generation latency test" << endl;
        cout << "3. Tamper detection accuracy" << endl;
        cout << "4. Root consistency test" << endl;
        cout << "Enter test case number: ";

        int testCase;
        cin >> testCase;
        cin.ignore();

        switch (testCase)
        {
        case 1:
            runMillionRecordTest();
            break;
        case 2:
            runProofLatencyTest();
            break;
        case 3:
            runTamperDetectionTest();
            break;
        case 4:
            runRootConsistencyTest();
            break;
        default:
            cout << "Invalid test case." << endl;
        }
    }

    void runMillionRecordTest()
    {
        cout << "Loading 1,000,000 records from Electronics.json..." << endl;
        if (dataProcessor.loadFromJSON("data/Electronics_5.json", 1000000)) {
            currentReviews = dataProcessor.getReviews();
            buildMerkleTree();
            cout << "1M record test completed" << endl;
        }
    }

    void runProofLatencyTest()
    {
        if (!treeBuilt || !existenceProver)
        {
            cout << "Please build Merkle tree first." << endl;
            return;
        }

        cout << "\nProof Latency Test" << endl;
        cout << "Testing 1000 random proofs..." << endl;
        existenceProver->benchmarkProofSystem(1000);
    }

    void runTamperDetectionTest()
    {
        if (!treeBuilt || !tamperDetector)
        {
            cout << "Please build Merkle tree first." << endl;
            return;
        }

        cout << "\nTamper Detection Accuracy Test" << endl;

        int successCount = 0;
        int totalTests = 5;

        // test modification
        vector<Review> modified = tamperDetector->tamperWithReviews(currentReviews, 1);
        vector<string> modData, modIds;
        for (const auto& review : modified) {
            modData.push_back(review.convertToString());
            modIds.push_back(review.getUniqueID());
        }
        MerkleTree modTree;
        modTree.buildTreeFromReviews(modData, modIds);
        TamperResult modResult = tamperDetector->detectByRootComparison(modTree.getRootHash());
        if (modResult.tamperingDetected) successCount++;

        // test deletion
        vector<Review> deleted = tamperDetector->deleteReviews(currentReviews, 1);
        vector<string> delData, delIds;
        for (const auto& review : deleted) {
            delData.push_back(review.convertToString());
            delIds.push_back(review.getUniqueID());
        }
        MerkleTree delTree;
        delTree.buildTreeFromReviews(delData, delIds);
        TamperResult delResult = tamperDetector->detectByRootComparison(delTree.getRootHash());
        if (delResult.tamperingDetected) successCount++;

        // test injection
        vector<Review> injected = tamperDetector->injectReviews(currentReviews, 1);
        vector<string> injData, injIds;
        for (const auto& review : injected) {
            injData.push_back(review.convertToString());
            injIds.push_back(review.getUniqueID());
        }
        MerkleTree injTree;
        injTree.buildTreeFromReviews(injData, injIds);
        TamperResult injResult = tamperDetector->detectByRootComparison(injTree.getRootHash());
        if (injResult.tamperingDetected) successCount++;

        // test rating manipulation
        vector<Review> rated = tamperDetector->manipulateRatings(currentReviews, 1);
        vector<string> rateData, rateIds;
        for (const auto& review : rated) {
            rateData.push_back(review.convertToString());
            rateIds.push_back(review.getUniqueID());
        }
        MerkleTree rateTree;
        rateTree.buildTreeFromReviews(rateData, rateIds);
        TamperResult rateResult = tamperDetector->detectByRootComparison(rateTree.getRootHash());
        if (rateResult.tamperingDetected) successCount++;

        // test character modification
        if (!currentReviews.empty()) {
            vector<Review> charMod = currentReviews;
            charMod[0].reviewText[0] = 'X';
            vector<string> charData, charIds;
            for (const auto& review : charMod) {
                charData.push_back(review.convertToString());
                charIds.push_back(review.getUniqueID());
            }
            MerkleTree charTree;
            charTree.buildTreeFromReviews(charData, charIds);
            TamperResult charResult = tamperDetector->detectByRootComparison(charTree.getRootHash());
            if (charResult.tamperingDetected) successCount++;
        }

        double accuracy = (successCount * 100.0 / totalTests);
        recordMetric("Tamper Detection Accuracy", to_string(accuracy) + "%");
        recordMetric("Tamper Detection Tests", to_string(successCount) + "/" + to_string(totalTests));

        cout << "Tamper detection accuracy: " << successCount << "/" << totalTests << " ("
            << accuracy << "%)" << endl;
    }

    void runRootConsistencyTest()
    {
        if (!treeBuilt)
        {
            cout << "Please build Merkle tree first." << endl;
            return;
        }

        cout << "\nRoot Consistency Test" << endl;

        string originalRoot = merkleTree.getRootHash();
        cout << "Original root: " << originalRoot.substr(0, 64) << "..." << endl;

        MerkleTree newTree;
        newTree.buildTreeFromReviews(reviewData, reviewIds);
        string newRoot = newTree.getRootHash();
        cout << "New root: " << newRoot.substr(0, 64) << "..." << endl;

        bool consistent = (originalRoot == newRoot);
        recordMetric("Root Consistency", consistent ? "PASSED" : "FAILED");

        if (consistent)
        {
            cout << "Root consistency: PASSED" << endl;
        }
        else
        {
            cout << "Root consistency: FAILED" << endl;
        }
    }

    void initializeMetricsFile()
    {
        metricsFile.open(metricsFilename, ios::app);
        if (metricsFile.is_open())
        {
            metricsFile << "Review Integrity System Metrics" << endl;
            metricsFile << "Timestamp: " << getCurrentTimestamp() << endl;
            metricsFile << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
            metricsFile << "Metric Description" << endl;
            metricsFile << "Hash Time (avg) - SHA-256 per record" << endl;
            metricsFile << "Proof Generation Time - Time to verify existence" << endl;
            metricsFile << "Total Build Time - Time to construct Merkle Tree" << endl;
            metricsFile << "Memory Usage - Peak memory during build" << endl;
            metricsFile << "Tamper Detection Accuracy - Ability to detect modifications" << endl;
            metricsFile << "Root Consistency - Root stability on unchanged data" << endl;
            metricsFile << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl << endl;
        }
    }

    string getCurrentTimestamp()
    {
        auto now = chrono::system_clock::now();
        auto time_t = chrono::system_clock::to_time_t(now);
        stringstream ss;

        struct tm timeinfo;
        localtime_s(&timeinfo, &time_t);
        ss << put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");

        return ss.str();
    }

    void recordMetric(const string& metricName, const string& value, const string& unit = "")
    {
        if (metricsFile.is_open())
        {
            metricsFile << "[" << getCurrentTimestamp() << "] " << metricName << ": " << value;
            if (!unit.empty()) metricsFile << " " << unit;
            metricsFile << endl;
            metricsFile.flush();
        }

        //also print to console
        cout << metricName << ": " << value;
        if (!unit.empty()) cout << " " << unit;
        cout << endl;
    }

    void runHashPerformanceTest()
    {
        cout << "\nHash Performance Test" << endl;
        cout << "Testing SHA-256 performance on sample data..." << endl;

        vector<string> testData;
        for (int i = 0; i < 1000 && i < reviewData.size(); i++)
        {
            testData.push_back(reviewData[i]);
        }

        auto start = chrono::high_resolution_clock::now();

        for (const auto& data : testData)
        {
            string hash = merkleTree.computeHash(data);
        }

        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::microseconds>(end - start);

        double avgHashTime = duration.count() / (double)testData.size();
        recordMetric("Hash Time (SHA-256 avg)", to_string(avgHashTime), "microseconds");
        recordMetric("Hash Performance", to_string(1000000.0 / avgHashTime), "hashes/sec");

        cout << "Average hash time: " << avgHashTime << " microseconds" << endl;
        cout << "Hash performance: " << (1000000.0 / avgHashTime) << " hashes/sec" << endl;
    }

    void exportMetrics()
    {
        if (metricsFile.is_open())
        {
            metricsFile.close();
        }

        // reopen to show final summary
        metricsFile.open(metricsFilename, ios::app);
        if (metricsFile.is_open())
        {
            metricsFile << "\n=== Metrics Export Completed ===" << endl;
            metricsFile << "Total records processed: " << currentReviews.size() << endl;
            metricsFile << "Final timestamp: " << getCurrentTimestamp() << endl;
            metricsFile << "=================================" << endl;
            metricsFile.close();
        }

        cout << "Metrics exported to: " << metricsFilename << endl;
    }
};