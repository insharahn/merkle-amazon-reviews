#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <chrono>
#include "MerkleTree.h"

using namespace std;

struct ProofResult
{
    string reviewId;
    string reviewData;
    string status;
    vector<string> proofPath;
    bool verified = false;
    long long proofTimeMicros = 0;
    long long verificationTimeMicros = 0;
    chrono::system_clock::time_point timestamp;

    void print() const
    {
        cout << "Proof Result:" << endl;
        cout << "  Review ID: " << reviewId << endl;
        cout << "  Status: " << status << endl;
        if (!proofPath.empty())
        {
            cout << "  Proof Size: " << proofPath.size() << " elements" << endl;
            cout << "  Verified: " << (verified ? "YES" : "NO") << endl;
        }
        if (proofTimeMicros > 0)
        {
            cout << "  Generation Time: " << proofTimeMicros << " microseconds" << endl;
        }
        if (verificationTimeMicros > 0)
        {
            cout << "  Verification Time: " << verificationTimeMicros << " microseconds" << endl;
        }
        cout << endl;
    }
};

class ExistenceProof
{
private:
    MerkleTree* merkleTree;
    unordered_map<string, string> reviewIdToData;
    unordered_map<string, vector<string>> productToReviewIds;

public:
    ExistenceProof(MerkleTree* tree) : merkleTree(tree) {}

    void indexReviews(const vector<Review>& reviews)
    {
        reviewIdToData.clear();
        productToReviewIds.clear();

        for (const auto& review : reviews)
        {
            string reviewData = review.convertToString();
            string reviewId = review.getUniqueID();
            string productId = review.asin;

            reviewIdToData[reviewId] = reviewData;
            productToReviewIds[productId].push_back(reviewId);
        }

        cout << "Indexed " << reviewIdToData.size() << " reviews for "
            << productToReviewIds.size() << " products" << endl;
    }

    ProofResult generateReviewProof(const string& reviewId)
    {
        ProofResult result;
        result.reviewId = reviewId;
        result.timestamp = chrono::system_clock::now();

        auto start = chrono::high_resolution_clock::now();

        auto dataIt = reviewIdToData.find(reviewId);
        if (dataIt == reviewIdToData.end())
        {
            result.status = "REVIEW_NOT_FOUND";
            result.proofTimeMicros = 0;
            return result;
        }

        result.reviewData = dataIt->second;
        vector<string> proof = merkleTree->generateProof(reviewId);
        result.proofPath = proof;

        auto end = chrono::high_resolution_clock::now();
        result.proofTimeMicros = chrono::duration_cast<chrono::microseconds>(end - start).count();

        if (proof.empty())
        {
            result.status = "PROOF_GENERATION_FAILED";
        }
        else
        {
            result.status = "PROOF_GENERATED";
            result.verified = MerkleTree::verifyProof(result.reviewData, proof, merkleTree->getRootHash());
        }

        return result;
    }

    vector<ProofResult> generateProductProofs(const string& productId)
    {
        vector<ProofResult> results;

        auto productIt = productToReviewIds.find(productId);
        if (productIt == productToReviewIds.end())
        {
            ProofResult emptyResult;
            emptyResult.status = "PRODUCT_NOT_FOUND";
            results.push_back(emptyResult);
            return results;
        }

        cout << "Generating proofs for product " << productId
            << " (" << productIt->second.size() << " reviews)" << endl;

        for (const string& reviewId : productIt->second)
        {
            results.push_back(generateReviewProof(reviewId));
        }

        return results;
    }

    vector<ProofResult> batchGenerateProofs(const vector<string>& reviewIds)
    {
        vector<ProofResult> results;
        results.reserve(reviewIds.size());

        cout << "Generating batch proofs for " << reviewIds.size() << " reviews" << endl;

        long long totalTime = 0;
        int successCount = 0;

        for (const string& reviewId : reviewIds)
        {
            ProofResult result = generateReviewProof(reviewId);
            results.push_back(result);

            if (result.status == "PROOF_GENERATED")
            {
                successCount++;
                totalTime += result.proofTimeMicros;
            }
        }

        if (successCount > 0)
        {
            cout << "Batch complete: " << successCount << "/" << reviewIds.size()
                << " proofs generated successfully" << endl;
            cout << "Average proof time: " << (totalTime / successCount) << " microseconds" << endl;
        }

        return results;
    }

    static ProofResult verifyProofExternally(const string& reviewData, const vector<string>& proof, const string& rootHash)
    {
        ProofResult result;
        result.timestamp = chrono::system_clock::now();

        auto start = chrono::high_resolution_clock::now();
        result.verified = MerkleTree::verifyProof(reviewData, proof, rootHash);
        auto end = chrono::high_resolution_clock::now();

        result.verificationTimeMicros = chrono::duration_cast<chrono::microseconds>(end - start).count();
        result.status = result.verified ? "VERIFICATION_SUCCESS" : "VERIFICATION_FAILED";

        return result;
    }

    void benchmarkProofSystem(int sampleSize = 100)
    {
        if (reviewIdToData.empty())
        {
            cout << "No reviews indexed for benchmarking" << endl;
            return;
        }

        cout << "\n=== Proof System Benchmark ===" << endl;
        cout << "Testing " << sampleSize << " random proofs..." << endl;

        vector<string> testIds;
        for (const auto& pair : reviewIdToData)
        {
            testIds.push_back(pair.first);
            if (testIds.size() >= sampleSize) break;
        }

        long long totalGenTime = 0;
        int genSuccessCount = 0;

        for (const string& id : testIds)
        {
            ProofResult result = generateReviewProof(id);
            if (result.status == "PROOF_GENERATED")
            {
                totalGenTime += result.proofTimeMicros;
                genSuccessCount++;
            }
        }

        long long totalVerifyTime = 0;
        int verifySuccessCount = 0;

        for (const string& id : testIds)
        {
            ProofResult result = generateReviewProof(id);
            if (result.status == "PROOF_GENERATED")
            {
                auto verifyResult = verifyProofExternally(result.reviewData, result.proofPath, merkleTree->getRootHash());
                totalVerifyTime += verifyResult.verificationTimeMicros;
                if (verifyResult.verified) verifySuccessCount++;
            }
        }

        cout << "Proof Generation:" << endl;
        cout << "  Success Rate: " << genSuccessCount << "/" << sampleSize << " ("
            << (genSuccessCount * 100.0 / sampleSize) << "%)" << endl;
        cout << "  Average Time: " << (genSuccessCount > 0 ? totalGenTime / genSuccessCount : 0)
            << " microseconds" << endl;

        cout << "Proof Verification:" << endl;
        cout << "  Success Rate: " << verifySuccessCount << "/" << sampleSize << " ("
            << (verifySuccessCount * 100.0 / sampleSize) << "%)" << endl;
        cout << "  Average Time: " << (verifySuccessCount > 0 ? totalVerifyTime / verifySuccessCount : 0)
            << " microseconds" << endl;

        long long avgGenTime = (genSuccessCount > 0 ? totalGenTime / genSuccessCount : 0);
        if (avgGenTime < 100000)
        {
            cout << "REQUIREMENT MET: Proof generation < 100ms" << endl;
        }
        else
        {
            cout << "REQUIREMENT FAILED: Proof generation > 100ms" << endl;
        }
    }

    bool reviewExists(const string& reviewId) const
    {
        return reviewIdToData.find(reviewId) != reviewIdToData.end();
    }

    bool productExists(const string& productId) const
    {
        return productToReviewIds.find(productId) != productToReviewIds.end();
    }

    vector<string> getProductReviews(const string& productId) const
    {
        auto it = productToReviewIds.find(productId);
        return (it != productToReviewIds.end()) ? it->second : vector<string>();
    }

    size_t getTotalIndexedReviews() const { return reviewIdToData.size(); }
    size_t getTotalIndexedProducts() const { return productToReviewIds.size(); }
};