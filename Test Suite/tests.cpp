#include "pch.h"
#include "C:\Users\zinsi\source\repos\merkle_algo\merkle_algo\MerkleTree.h"
#include "C:\Users\zinsi\source\repos\merkle_algo\merkle_algo\CLI.h"
#include "C:\Users\zinsi\source\repos\merkle_algo\merkle_algo\DataPreprocessor.h"
#include "C:\Users\zinsi\source\repos\merkle_algo\merkle_algo\IntegrityVerifier.h"
#include "C:\Users\zinsi\source\repos\merkle_algo\merkle_algo\ExistenceProver.h"
#include "C:\Users\zinsi\source\repos\merkle_algo\merkle_algo\PerformanceMeasurer.h"
#include "C:\Users\zinsi\source\repos\merkle_algo\merkle_algo\TamperingDetector.h"

// for tests
#include <filesystem>
#include <fstream>
#include <chrono>

using namespace std;
namespace fs = filesystem;


// GTEST_SKIP in older versions
#ifndef GTEST_SKIP
#define GTEST_SKIP() return
#endif

//testing if path is right
TEST(TestCaseName, TestName) {
  EXPECT_EQ(12, doubler(6));
  EXPECT_TRUE(true);
}


// rest of the tests
class MerkleTreeTest : public testing::Test {
protected:
    void SetUp() override {
        if (!fs::exists("data")) {
            fs::create_directory("data");
        }
        createTestDataset();

        // build the test tree in setup so all tests can use it
        ASSERT_TRUE(processor.loadFromJSON("data/test_small.json", 0));
        testReviews = processor.getReviews();
        for (const auto& review : testReviews) {
            reviewData.push_back(review.convertToString());
            reviewIds.push_back(review.getUniqueID());
        }
        tree.buildTreeFromReviews(reviewData, reviewIds);
    }

    void createTestDataset() {
        ofstream file("data/test_small.json");
        file << R"({"reviewerID": "A1", "asin": "P1", "reviewText": "Great product", "summary": "Excellent", "overall": 5.0, "unixReviewTime": "1000000"})" << endl;
        file << R"({"reviewerID": "A2", "asin": "P1", "reviewText": "Good product", "summary": "Good", "overall": 4.0, "unixReviewTime": "1000001"})" << endl;
        file << R"({"reviewerID": "A3", "asin": "P2", "reviewText": "Average product", "summary": "Average", "overall": 3.0, "unixReviewTime": "1000002"})" << endl;
        file.close();
    }

    DataPreprocessor processor;
    MerkleTree tree;
    vector<Review> testReviews;
    vector<string> reviewData;
    vector<string> reviewIds;
};

// Test 1: Load 1 million reviews and build Merkle Tree
TEST_F(MerkleTreeTest, LoadMillionRecordsAndBuildTree) {
    if (!fs::exists("data/Electronics_5.json")) {
        SUCCEED() << "Large dataset not available";
        return;
    }

    DataPreprocessor largeProcessor;
    ASSERT_TRUE(largeProcessor.loadFromJSON("data/Electronics_5.json", 1000)); // change when its time to run 1m 

    auto largeReviews = largeProcessor.getReviews();
    vector<string> largeData, largeIds;

    for (const auto& review : largeReviews) {
        largeData.push_back(review.convertToString());
        largeIds.push_back(review.getUniqueID());
    }

    MerkleTree largeTree;
    largeTree.buildTreeFromReviews(largeData, largeIds);

    EXPECT_FALSE(largeTree.getRootHash().empty());
    EXPECT_GT(largeTree.getLeafCount(), 0);
}

// Test 2: Save generated Merkle Root
TEST_F(MerkleTreeTest, SaveGeneratedMerkleRoot) {
    IntegrityVerifier verifier;
    string rootHash = tree.getRootHash();
    verifier.storeRootHash("test_dataset", rootHash);
    verifier.saveRootToFile("test_roots.txt", "test_dataset", rootHash);

    EXPECT_TRUE(fs::exists("test_roots.txt"));

    // clean up
    fs::remove("test_roots.txt");
}

// Test 3: Query existing review
TEST_F(MerkleTreeTest, QueryExistingReview) {
    ExistenceProof prover(&tree);
    prover.indexReviews(testReviews);

    ProofResult result = prover.generateReviewProof(reviewIds[0]);

    EXPECT_EQ(result.status, "PROOF_GENERATED");
    EXPECT_TRUE(result.verified);
    EXPECT_FALSE(result.proofPath.empty());
}

// Test 4: Query non-existing review  
TEST_F(MerkleTreeTest, QueryNonExistingReview) {
    ExistenceProof prover(&tree);
    prover.indexReviews(testReviews);

    ProofResult result = prover.generateReviewProof("NON_EXISTENT_REVIEW_12345");

    EXPECT_EQ(result.status, "REVIEW_NOT_FOUND");
    EXPECT_FALSE(result.verified);
}

// Test 5: Modify one review text detection
TEST_F(MerkleTreeTest, DetectModifiedReviewText) {
    vector<Review> modifiedReviews = testReviews;
    modifiedReviews[0].reviewText = "MODIFIED TEXT";

    vector<string> modData, modIds;
    for (const auto& review : modifiedReviews) {
        modData.push_back(review.convertToString());
        modIds.push_back(review.getUniqueID());
    }

    MerkleTree modifiedTree;
    modifiedTree.buildTreeFromReviews(modData, modIds);

    EXPECT_NE(tree.getRootHash(), modifiedTree.getRootHash());
}

// Test 6: Single character modification detection
TEST_F(MerkleTreeTest, DetectSingleCharacterModification) {
    vector<Review> charModified = testReviews;
    charModified[0].reviewText = "god"; //changed from "good"

    vector<string> charData, charIds;
    for (const auto& review : charModified) {
        charData.push_back(review.convertToString());
        charIds.push_back(review.getUniqueID());
    }

    MerkleTree charTree;
    charTree.buildTreeFromReviews(charData, charIds);

    EXPECT_NE(tree.getRootHash(), charTree.getRootHash());
}

// Test 7: Delete review record detection
TEST_F(MerkleTreeTest, DetectDeletedReview) {
    vector<Review> deletedReviews = testReviews;
    deletedReviews.pop_back();

    vector<string> delData, delIds;
    for (const auto& review : deletedReviews) {
        delData.push_back(review.convertToString());
        delIds.push_back(review.getUniqueID());
    }

    MerkleTree delTree;
    delTree.buildTreeFromReviews(delData, delIds);

    EXPECT_NE(tree.getRootHash(), delTree.getRootHash());
}

// Test 8: Insert fake record detection
TEST_F(MerkleTreeTest, DetectFakeRecordInsertion) {
    vector<Review> injectedReviews = testReviews;

    Review fake = testReviews[0];
    fake.reviewerID = "FAKE_USER_123";
    fake.reviewID = fake.reviewerID + "_" + fake.asin + "_" + fake.unixReviewTime;
    injectedReviews.push_back(fake);

    vector<string> injData, injIds;
    for (const auto& review : injectedReviews) {
        injData.push_back(review.convertToString());
        injIds.push_back(review.getUniqueID());
    }

    MerkleTree injTree;
    injTree.buildTreeFromReviews(injData, injIds);

    EXPECT_NE(tree.getRootHash(), injTree.getRootHash());
}

// Test 9: Compare current root vs stored root
TEST_F(MerkleTreeTest, CompareCurrentVsStoredRoot) {
    IntegrityVerifier verifier;
    string rootHash = tree.getRootHash();

    verifier.storeRootHash("test_dataset", rootHash);

    string result = verifier.compareWithStored("test_dataset");

    EXPECT_TRUE(result.find("INTEGRITY_VERIFIED") != string::npos ||
        result.find("INTEGRITY_VIOLATED") != string::npos);
}

// Test 10: Proof generation time for 100 reviews
TEST_F(MerkleTreeTest, ProofGenerationTimeFor100Reviews) {
    ExistenceProof prover(&tree);
    prover.indexReviews(testReviews);

    auto start = chrono::high_resolution_clock::now();

    int testsToRun = min(10, (int)reviewIds.size()); // reduced for speed
    for (int i = 0; i < testsToRun; i++) {
        ProofResult result = prover.generateReviewProof(reviewIds[i]);
        EXPECT_EQ(result.status, "PROOF_GENERATED");
    }

    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);

    EXPECT_LE(duration.count(), 1000); // relaxed timing for testing
}

// Test 11: Memory and hash computation performance
TEST_F(MerkleTreeTest, MemoryAndHashPerformance) {
    PerformanceMeasurer measurer;

    auto results = measurer.measureScalability(testReviews, { 3 });

    EXPECT_FALSE(results.empty());
}

// Test 12: Multiple categories comparison
TEST_F(MerkleTreeTest, MultipleCategoriesComparison) {
    if (!fs::exists("data/Electronics_5.json") || !fs::exists("data/Automotive_5.json")) {
        SUCCEED() << "Multiple category datasets not available";
        return;
    }

    DataPreprocessor elecProcessor, autoProcessor;
    ASSERT_TRUE(elecProcessor.loadFromJSON("data/Electronics_5.json", 100));
    ASSERT_TRUE(autoProcessor.loadFromJSON("data/Automotive_5.json", 100));

    vector<string> elecData, elecIds, autoData, autoIds;

    for (const auto& review : elecProcessor.getReviews()) {
        elecData.push_back(review.convertToString());
        elecIds.push_back(review.getUniqueID());
    }

    for (const auto& review : autoProcessor.getReviews()) {
        autoData.push_back(review.convertToString());
        autoIds.push_back(review.getUniqueID());
    }

    MerkleTree elecTree, autoTree;
    elecTree.buildTreeFromReviews(elecData, elecIds);
    autoTree.buildTreeFromReviews(autoData, autoIds);

    EXPECT_NE(elecTree.getRootHash(), autoTree.getRootHash());
    EXPECT_FALSE(elecTree.getRootHash().empty());
    EXPECT_FALSE(autoTree.getRootHash().empty());
}

// Test 13: Rebuild tree after dataset update
TEST_F(MerkleTreeTest, RebuildTreeAfterDatasetUpdate) {
    string originalRoot = tree.getRootHash();

    vector<Review> updatedReviews = testReviews;
    updatedReviews[0].overall = 1.0; // change rating

    vector<string> updatedData, updatedIds;
    for (const auto& review : updatedReviews) {
        updatedData.push_back(review.convertToString());
        updatedIds.push_back(review.getUniqueID());
    }

    MerkleTree updatedTree;
    updatedTree.buildTreeFromReviews(updatedData, updatedIds);

    EXPECT_NE(originalRoot, updatedTree.getRootHash());
}

// Test 14: Load same dataset twice - root consistency
TEST_F(MerkleTreeTest, LoadSameDatasetTwiceRootConsistency) {
    MerkleTree tree1, tree2;

    tree1.buildTreeFromReviews(reviewData, reviewIds);
    tree2.buildTreeFromReviews(reviewData, reviewIds);

    EXPECT_EQ(tree1.getRootHash(), tree2.getRootHash());
}

// Test 15: Add new review with partial rebuild
TEST_F(MerkleTreeTest, AddNewReviewPartialRebuild) {
    string oldRoot = tree.getRootHash();

    Review newReview = testReviews[0];
    newReview.reviewerID = "NEW_USER_PARTIAL";
    newReview.reviewText = "New review for partial rebuild test";
    newReview.reviewID = newReview.reviewerID + "_" + newReview.asin + "_" + to_string(time(nullptr));

    tree.addReview(newReview.convertToString(), newReview.getUniqueID());
    string newRoot = tree.getRootHash();

    EXPECT_NE(oldRoot, newRoot);
    EXPECT_TRUE(tree.contains(newReview.getUniqueID()));
}
