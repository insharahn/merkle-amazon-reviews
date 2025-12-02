#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <random>
#include <algorithm>
#include <ctime>
#include "MerkleTree.h"

using namespace std;

struct TamperResult
{
    string detectionMethod;
    string status;
    bool tamperingDetected = false;
    string originalRoot;
    string newRoot;

    void print() const
    {
        cout << "Tamper Detection Result:" << endl;
        cout << "  Method: " << detectionMethod << endl;
        cout << "  Status: " << status << endl;
        cout << "  Tampering: " << (tamperingDetected ? "DETECTED" : "NOT DETECTED") << endl;
        if (!originalRoot.empty())
        {
            cout << "  Original Root: " << originalRoot.substr(0, 16) << "..." << endl;
        }
        if (!newRoot.empty())
        {
            cout << "  New Root: " << newRoot.substr(0, 16) << "..." << endl;
        }
        cout << endl;
    }
};

struct ReviewTamperResult
{
    string reviewId;
    string status;
    bool tampered = false;

    void print() const
    {
        cout << "  Review: " << reviewId << " - " << status << endl;
    }
};

struct ComprehensiveTamperReport
{
    TamperResult rootComparison;
    vector<ReviewTamperResult> modifiedReviews;
    string analysis;
    int originalReviewCount = 0;
    int newReviewCount = 0;
    int tamperedReviewCount = 0;

    void print() const
    {
        cout << "\n=== COMPREHENSIVE TAMPER ANALYSIS ===" << endl;
        cout << "Original Reviews: " << originalReviewCount << endl;
        cout << "New Reviews: " << newReviewCount << endl;
        cout << "Tampered Reviews: " << tamperedReviewCount << endl;

        rootComparison.print();

        cout << "Detailed Analysis:" << endl;
        cout << analysis << endl;

        if (tamperedReviewCount > 0)
        {
            cout << "Modified Reviews:" << endl;
            for (const auto& result : modifiedReviews)
            {
                if (result.tampered)
                {
                    result.print();
                }
            }
        }
    }
};

class TamperDetector
{
private:
    MerkleTree* originalTree;
    vector<Review> originalReviews;
    unordered_map<string, string> originalRoots;
    string currentDatasetName;

    Review createFakeReview()
    {
        static int fakeCounter = 0;
        fakeCounter++;

        Review fakeReview = originalReviews[0];
        fakeReview.reviewerID = "FAKE_USER_" + to_string(fakeCounter);
        fakeReview.asin = "FAKE_PRODUCT_" + to_string(fakeCounter);
        fakeReview.reviewText = "This is a fake injected review for testing tamper detection.";
        fakeReview.summary = "Fake Review";
        fakeReview.overall = 5.0;
        fakeReview.unixReviewTime = to_string(time(nullptr));
        fakeReview.reviewID = fakeReview.reviewerID + "_" + fakeReview.asin + "_" + fakeReview.unixReviewTime;

        return fakeReview;
    }

public:
    TamperDetector(MerkleTree* tree, const vector<Review>& reviews)
        : originalTree(tree), originalReviews(reviews)
    {
    }

    void setDatasetName(const string& name)
    {
        currentDatasetName = name;
        originalRoots[name] = originalTree->getRootHash();
    }

    vector<Review> tamperWithReviews(const vector<Review>& reviews, int numModifications = 1)
    {
        vector<Review> tampered = reviews;
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> dis(0, static_cast<int>(tampered.size()) - 1);

        cout << "Simulating " << numModifications << " review modification(s)..." << endl;

        for (int i = 0; i < numModifications; i++)
        {
            int index = dis(gen);
            tampered[index].reviewText += " [TAMPERED]";
            cout << "  Modified review: " << tampered[index].getUniqueID() << endl;
        }

        return tampered;
    }

    vector<Review> deleteReviews(const vector<Review>& reviews, int numDeletions = 1)
    {
        if (numDeletions >= reviews.size())
        {
            cout << "Warning: Cannot delete all reviews" << endl;
            return reviews;
        }

        vector<Review> tampered = reviews;
        random_device rd;
        mt19937 gen(rd());

        cout << "Simulating " << numDeletions << " review deletion(s)..." << endl;

        for (int i = 0; i < numDeletions; i++)
        {
            uniform_int_distribution<> dis(0, static_cast<int>(tampered.size()) - 1);
            int index = dis(gen);
            cout << "  Deleted review: " << tampered[index].getUniqueID() << endl;
            tampered.erase(tampered.begin() + index);
        }

        return tampered;
    }

    vector<Review> injectReviews(const vector<Review>& reviews, int numInjections = 1)
    {
        cout << "Simulating " << numInjections << " review injection(s)..." << endl;
        random_device rd;
        vector<Review> tampered = reviews;
        mt19937 gen(rd());

        for (int i = 0; i < numInjections; i++)
        {
            Review fake = createFakeReview();
            tampered.push_back(fake);
            cout << "  Injected fake review: " << fake.getUniqueID() << endl;
        }

        return tampered;
    }

    vector<ReviewTamperResult> detectModifiedReviews(const vector<Review>& reviews, MerkleTree& newTree)
    {
        vector<ReviewTamperResult> results;

        cout << "Scanning for modified reviews..." << endl;

        for (const auto& review : reviews)
        {
            ReviewTamperResult result;
            result.reviewId = review.getUniqueID();

            if (!originalTree->contains(review.getUniqueID()))
            {
                result.status = "NEW_REVIEW_DETECTED";
                result.tampered = true;
                results.push_back(result);
                continue;
            }

            string reviewData = review.convertToString();
            vector<string> proof = newTree.generateProof(review.getUniqueID());

            if (proof.empty())
            {
                result.status = "PROOF_GENERATION_FAILED";
                result.tampered = true;
            }
            else
            {
                bool valid = MerkleTree::verifyProof(reviewData, proof, newTree.getRootHash());
                if (!valid)
                {
                    result.status = "MODIFIED_REVIEW_DETECTED";
                    result.tampered = true;
                }
                else
                {
                    result.status = "REVIEW_VALID";
                    result.tampered = false;
                }
            }

            results.push_back(result);
        }

        return results;
    }

    void storeOriginalRoot(const string& datasetName, const string& rootHash)
    {
        originalRoots[datasetName] = rootHash;
    }

    vector<Review> manipulateRatings(const vector<Review>& reviews, int numChanges = 1)
    {
        vector<Review> tampered = reviews;
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> indexDis(0, static_cast<int>(tampered.size()) - 1);
        uniform_real_distribution<> ratingDis(1.0, 5.0);

        cout << "Simulating " << numChanges << " rating manipulation(s)..." << endl;

        for (int i = 0; i < numChanges; i++)
        {
            int index = indexDis(gen);
            double oldRating = tampered[index].overall;
            double newRating = ratingDis(gen);
            tampered[index].overall = newRating;
            cout << "  Changed rating from " << oldRating << " to " << newRating
                << " for review: " << tampered[index].getUniqueID() << endl;
        }

        return tampered;
    }

    TamperResult detectByRootComparison(const string& newRootHash)
    {
        TamperResult result;
        result.detectionMethod = "ROOT_HASH_COMPARISON";

        if (currentDatasetName.empty() || originalRoots.find(currentDatasetName) == originalRoots.end())
        {
            result.status = "ERROR: No original root stored for comparison";
            return result;
        }

        string originalRoot = originalRoots[currentDatasetName];
        result.originalRoot = originalRoot;
        result.newRoot = newRootHash;

        if (originalRoot == newRootHash)
        {
            result.status = "NO_TAMPERING_DETECTED";
            result.tamperingDetected = false;
        }
        else
        {
            result.status = "TAMPERING_DETECTED: Root hash mismatch";
            result.tamperingDetected = true;
        }

        return result;
    }

    ComprehensiveTamperReport comprehensiveAnalysis(const vector<Review>& newReviews, MerkleTree& newTree)
    {
        ComprehensiveTamperReport report;
        report.originalReviewCount = static_cast<int>(originalReviews.size());
        report.newReviewCount = static_cast<int>(newReviews.size());

        report.rootComparison = detectByRootComparison(newTree.getRootHash());
        report.modifiedReviews = detectModifiedReviews(newReviews, newTree);

        report.tamperedReviewCount = count_if(report.modifiedReviews.begin(),
            report.modifiedReviews.end(),
            [](const ReviewTamperResult& r) { return r.tampered; });

        if (report.newReviewCount > report.originalReviewCount)
        {
            report.analysis += "INJECTION_DETECTED: " +
                to_string(report.newReviewCount - report.originalReviewCount) +
                " new reviews added\n";
        }
        else if (report.newReviewCount < report.originalReviewCount)
        {
            report.analysis += "DELETION_DETECTED: " +
                to_string(report.originalReviewCount - report.newReviewCount) +
                " reviews deleted\n";
        }

        if (report.tamperedReviewCount > 0)
        {
            report.analysis += "MODIFICATIONS_DETECTED: " +
                to_string(report.tamperedReviewCount) + " reviews modified\n";
        }

        if (report.rootComparison.tamperingDetected)
        {
            report.analysis += "INTEGRITY_VIOLATION: Root hash mismatch confirms tampering\n";
        }
        else if (report.tamperedReviewCount == 0 &&
            report.newReviewCount == report.originalReviewCount)
        {
            report.analysis += "INTEGRITY_PRESERVED: No tampering detected\n";
        }

        return report;
    }

    //getters
    string getOriginalRoot() const
    {
        auto it = originalRoots.find(currentDatasetName);
        return (it != originalRoots.end()) ? it->second : "";
    }
};