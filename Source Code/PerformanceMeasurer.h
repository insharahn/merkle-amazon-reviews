#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <fstream>
#include <algorithm>
#include <windows.h>
#include <psapi.h>
#include "MerkleTree.h"

using namespace std;

class PerformanceMeasurer
{
private:
    struct MeasurementResult
    {
        string testName;
        long long executionTimeMicroseconds;
        SIZE_T memoryUsageBytes;
        int datasetSize;
        string additionalInfo;

        void print() const
        {
            cout << "  " << testName << ":" << endl;
            cout << "    Time: " << executionTimeMicroseconds << " micros";
            if (executionTimeMicroseconds > 1000)
            {
                cout << " (" << executionTimeMicroseconds / 1000.0 << " ms)";
            }
            cout << endl;
            cout << "    Memory: " << memoryUsageBytes << " bytes";
            if (memoryUsageBytes > 1024)
            {
                cout << " (" << memoryUsageBytes / 1024.0 << " KB)";
            }
            if (memoryUsageBytes > 1024 * 1024)
            {
                cout << " (" << memoryUsageBytes / (1024.0 * 1024.0) << " MB)";
            }
            cout << endl;
            cout << "    Dataset: " << datasetSize << " reviews" << endl;
            if (!additionalInfo.empty())
            {
                cout << "    Info: " << additionalInfo << endl;
            }
            cout << endl;
        }
    };

    SIZE_T getCurrentMemoryUsage()
    {
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        {
            return pmc.WorkingSetSize;
        }
        return 0;
    }

    string getCurrentTimestamp()
    {
        auto now = chrono::system_clock::now();
        auto time_t = chrono::system_clock::to_time_t(now);
        char buffer[80];
        ctime_s(buffer, sizeof(buffer), &time_t);
        return string(buffer);
    }

public:
    MeasurementResult measureHashingSpeed(const vector<string>& data)
    {
        MeasurementResult result;
        result.testName = "SHA-256 Hashing Speed";
        result.datasetSize = static_cast<int>(data.size());

        auto startTime = chrono::high_resolution_clock::now();
        auto startMemory = getCurrentMemoryUsage();

        SHA256 hasher;
        vector<string> hashes;
        hashes.reserve(data.size());

        for (const auto& str : data)
        {
            hashes.push_back(hasher(str));
        }

        auto endTime = chrono::high_resolution_clock::now();
        auto endMemory = getCurrentMemoryUsage();

        result.executionTimeMicroseconds = chrono::duration_cast<chrono::microseconds>(endTime - startTime).count();
        result.memoryUsageBytes = endMemory - startMemory;
        result.additionalInfo = to_string(hashes.size()) + " hashes computed";

        return result;
    }

    MeasurementResult measureTreeConstruction(const vector<string>& data, const vector<string>& ids)
    {
        MeasurementResult result;
        result.testName = "Merkle Tree Construction";
        result.datasetSize = static_cast<int>(data.size());

        auto startTime = chrono::high_resolution_clock::now();
        auto startMemory = getCurrentMemoryUsage();

        MerkleTree tree;
        tree.buildTreeFromReviews(data, ids);

        auto endTime = chrono::high_resolution_clock::now();
        auto endMemory = getCurrentMemoryUsage();

        result.executionTimeMicroseconds = chrono::duration_cast<chrono::microseconds>(endTime - startTime).count();
        result.memoryUsageBytes = endMemory - startMemory;
        result.additionalInfo = "Root: " + tree.getRootHash().substr(0, 16) + "...";

        return result;
    }

    MeasurementResult measureProofGeneration(MerkleTree& tree, const vector<string>& reviewIds, int sampleSize = 100)
    {
        MeasurementResult result;
        result.testName = "Proof Generation (" + to_string(sampleSize) + " samples)";
        result.datasetSize = static_cast<int>(reviewIds.size());

        auto startTime = chrono::high_resolution_clock::now();
        auto startMemory = getCurrentMemoryUsage();

        long long totalTime = 0;
        int successCount = 0;

        for (int i = 0; i < min(sampleSize, static_cast<int>(reviewIds.size())); i++)
        {
            auto proofStart = chrono::high_resolution_clock::now();
            vector<string> proof = tree.generateProof(reviewIds[i]);
            auto proofEnd = chrono::high_resolution_clock::now();

            if (!proof.empty())
            {
                totalTime += chrono::duration_cast<chrono::microseconds>(proofEnd - proofStart).count();
                successCount++;
            }
        }

        auto endTime = chrono::high_resolution_clock::now();
        auto endMemory = getCurrentMemoryUsage();

        result.executionTimeMicroseconds = (successCount > 0) ? (totalTime / successCount) : 0;
        result.memoryUsageBytes = endMemory - startMemory;
        result.additionalInfo = "Average of " + to_string(successCount) + " successful proofs";

        return result;
    }

    MeasurementResult measureProofVerification(MerkleTree& tree, const vector<string>& reviewData,
        const vector<string>& reviewIds, int sampleSize = 100)
    {
        MeasurementResult result;
        result.testName = "Proof Verification (" + to_string(sampleSize) + " samples)";
        result.datasetSize = static_cast<int>(reviewData.size());

        auto startTime = chrono::high_resolution_clock::now();
        auto startMemory = getCurrentMemoryUsage();

        long long totalTime = 0;
        int successCount = 0;

        for (int i = 0; i < min(sampleSize, static_cast<int>(reviewData.size())); i++)
        {
            vector<string> proof = tree.generateProof(reviewIds[i]);
            if (proof.empty()) continue;

            auto verifyStart = chrono::high_resolution_clock::now();
            bool valid = MerkleTree::verifyProof(reviewData[i], proof, tree.getRootHash());
            auto verifyEnd = chrono::high_resolution_clock::now();

            if (valid)
            {
                totalTime += chrono::duration_cast<chrono::microseconds>(verifyEnd - verifyStart).count();
                successCount++;
            }
        }

        auto endTime = chrono::high_resolution_clock::now();
        auto endMemory = getCurrentMemoryUsage();

        result.executionTimeMicroseconds = (successCount > 0) ? (totalTime / successCount) : 0;
        result.memoryUsageBytes = endMemory - startMemory;
        result.additionalInfo = "Average of " + to_string(successCount) + " successful verifications";

        return result;
    }

    vector<MeasurementResult> measureScalability(const vector<Review>& allReviews,
        const vector<int>& datasetSizes = { 100, 1000, 5000, 10000 })
    {
        vector<MeasurementResult> results;

        cout << "=== Scalability Analysis ===" << endl;
        cout << "Testing dataset sizes: ";
        for (int size : datasetSizes)
        {
            cout << size << " ";
        }
        cout << endl << endl;

        for (int size : datasetSizes)
        {
            if (size > allReviews.size())
            {
                cout << "Skipping size " << size << " (insufficient data)" << endl;
                continue;
            }

            cout << "Testing with " << size << " reviews..." << endl;

            vector<string> subsetData, subsetIds;
            for (int i = 0; i < size; i++)
            {
                subsetData.push_back(allReviews[i].convertToString());
                subsetIds.push_back(allReviews[i].getUniqueID());
            }

            auto constructionResult = measureTreeConstruction(subsetData, subsetIds);
            constructionResult.testName = "Construction [" + to_string(size) + " reviews]";
            results.push_back(constructionResult);

            MerkleTree tree;
            tree.buildTreeFromReviews(subsetData, subsetIds);

            auto proofResult = measureProofGeneration(tree, subsetIds, min(100, size));
            proofResult.testName = "Proof Generation [" + to_string(size) + " reviews]";
            results.push_back(proofResult);
        }

        return results;
    }

    void runComprehensiveAnalysis(const vector<Review>& reviews)
    {
        cout << "\n=== COMPREHENSIVE PERFORMANCE ANALYSIS ===" << endl;

        vector<string> reviewData, reviewIds;
        for (const auto& review : reviews)
        {
            reviewData.push_back(review.convertToString());
            reviewIds.push_back(review.getUniqueID());
        }

        vector<MeasurementResult> allResults;

        cout << "\n1. Hashing Performance:" << endl;
        auto hashingResult = measureHashingSpeed(reviewData);
        hashingResult.print();
        allResults.push_back(hashingResult);

        cout << "2. Tree Construction Performance:" << endl;
        auto constructionResult = measureTreeConstruction(reviewData, reviewIds);
        constructionResult.print();
        allResults.push_back(constructionResult);

        MerkleTree tree;
        tree.buildTreeFromReviews(reviewData, reviewIds);

        cout << "3. Proof Generation Performance:" << endl;
        auto proofGenResult = measureProofGeneration(tree, reviewIds, 100);
        proofGenResult.print();
        allResults.push_back(proofGenResult);

        cout << "4. Proof Verification Performance:" << endl;
        auto proofVerResult = measureProofVerification(tree, reviewData, reviewIds, 100);
        proofVerResult.print();
        allResults.push_back(proofVerResult);

        validateRequirements(allResults);
        generatePerformanceReport(allResults, "performance_report.txt");
    }

    void validateRequirements(const vector<MeasurementResult>& results)
    {
        cout << "\n=== REQUIREMENT VALIDATION ===" << endl;

        bool allRequirementsMet = true;

        for (const auto& result : results)
        {
            if (result.testName.find("Proof Generation") != string::npos)
            {
                if (result.executionTimeMicroseconds > 100000)
                {
                    cout << "FAILED: Proof generation too slow: "
                        << result.executionTimeMicroseconds << " micros (requirement: < 100,000 micros)" << endl;
                    allRequirementsMet = false;
                }
                else
                {
                    cout << "PASSED: Proof generation " << result.executionTimeMicroseconds
                        << " micros < 100,000 micros requirement" << endl;
                }
            }

            if (result.testName.find("Construction") != string::npos && result.datasetSize >= 1000000)
            {
                cout << "PASSED: Can handle " << result.datasetSize << " records (requirement: ?1M)" << endl;
            }
        }

        if (allRequirementsMet)
        {
            cout << "\nALL PROJECT REQUIREMENTS MET!" << endl;
        }
        else
        {
            cout << "\nSOME REQUIREMENTS NOT MET" << endl;
        }
    }

    void generatePerformanceReport(const vector<MeasurementResult>& results, const string& filename)
    {
        ofstream file(filename);
        if (!file.is_open())
        {
            cout << "Error: Could not create performance report file" << endl;
            return;
        }

        file << "=== MERKLE TREE PERFORMANCE REPORT ===" << endl;
        file << "Generated: " << getCurrentTimestamp() << endl;
        file << endl;

        for (const auto& result : results)
        {
            file << result.testName << ":" << endl;
            file << "  Duration: " << result.executionTimeMicroseconds << " micros" << endl;
            file << "  Memory: " << result.memoryUsageBytes << " bytes" << endl;
            file << "  Dataset: " << result.datasetSize << " reviews" << endl;
            if (!result.additionalInfo.empty())
            {
                file << "  Info: " << result.additionalInfo << endl;
            }
            file << endl;
        }

        file.close();
        cout << "Performance report saved to: " << filename << endl;
    }
};