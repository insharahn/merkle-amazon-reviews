#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <queue>
#include <algorithm>
#include <chrono>
#include "SHA256.h"

using namespace std;

// ========================
// merkle node
// ========================
class MerkleNode
{
public:
    string hash;
    shared_ptr<MerkleNode> left;
    shared_ptr<MerkleNode> right;
    shared_ptr<MerkleNode> parent;
    string data; // leaf data

    // constructor for leaf node
    MerkleNode(const string& dataHash, const string& reviewId = "")
        : hash(dataHash), left(nullptr), right(nullptr), parent(nullptr), data(reviewId) {
    }

    // constructor for internal node
    MerkleNode(const string& nodeHash,
        shared_ptr<MerkleNode> leftChild,
        shared_ptr<MerkleNode> rightChild)
        : hash(nodeHash), left(leftChild), right(rightChild),
        parent(nullptr), data("") {
    }

    // check if node is leaf
    bool isLeaf() const { return left == nullptr && right == nullptr; }

    // get leaf's review id
    string getReviewId() const { return data; }
};

// ========================
// merkle tree
// ========================
class MerkleTree
{
public:
    shared_ptr<MerkleNode> root;
    unordered_map<string, shared_ptr<MerkleNode>> leafMap;
    SHA256 hasher;

    // compute hash of data
    string computeHash(const string& data)
    {
        return hasher(data);
    }
private:

    // combine two hashes in sorted order for consistency
    string combineHashes(const string& h1, const string& h2)
    {
        return (h1 < h2) ? computeHash(h1 + h2) : computeHash(h2 + h1);
    }

    // build internal levels of the tree
    shared_ptr<MerkleNode> buildTree(vector<shared_ptr<MerkleNode>>& nodes)
    {
        if (nodes.empty()) return nullptr;
        if (nodes.size() == 1) return nodes[0];

        vector<shared_ptr<MerkleNode>> current;
        current.swap(nodes);

        while (current.size() > 1)
        {
            vector<shared_ptr<MerkleNode>> next;
            next.reserve((current.size() + 1) / 2);

            for (size_t i = 0; i < current.size(); i += 2)
            {
                shared_ptr<MerkleNode> parent;

                if (i + 1 < current.size())
                {
                    string parentHash = combineHashes(current[i]->hash, current[i + 1]->hash);
                    parent = make_shared<MerkleNode>(parentHash, current[i], current[i + 1]);
                }
                else
                {
                    // duplicate last node if odd
                    string dupHash = combineHashes(current[i]->hash, current[i]->hash);
                    parent = make_shared<MerkleNode>(dupHash, current[i], current[i]);
                }

                parent->left->parent = parent;
                parent->right->parent = parent;

                next.push_back(move(parent));
            }

            current = move(next);
        }

        return current[0];
    }

public:
    MerkleTree() : root(nullptr) {}

    // build from review data and ids
    void buildTreeFromReviews(const vector<string>& reviewData,
        const vector<string>& reviewIds)
    {
        if (reviewData.size() != reviewIds.size())
        {
            throw invalid_argument("review data and id arrays must match in size");
        }

        leafMap.clear();
        vector<shared_ptr<MerkleNode>> leaves;
        leaves.reserve(reviewData.size());

        cout << "building merkle tree with " << reviewData.size() << " reviews..." << endl;

        unordered_map<string, int> duplicateTracker;
        int duplicateCount = 0;

        auto start = chrono::high_resolution_clock::now();

        for (size_t i = 0; i < reviewData.size(); i++)
        {
            string leafHash = computeHash(reviewData[i]);

            //handle duplicates by creating unique IDs
            string uniqueId = reviewIds[i];
            if (leafMap.find(uniqueId) != leafMap.end())
            {
                //generate unique ID for duplicate
                int suffix = 1;
                do {
                    uniqueId = reviewIds[i] + "_dup" + to_string(suffix++);
                } while (leafMap.find(uniqueId) != leafMap.end());

                duplicateTracker[reviewIds[i]]++;
                duplicateCount++;
            }

            auto leaf = make_shared<MerkleNode>(leafHash, uniqueId);
            leaves.push_back(leaf);
            leafMap[uniqueId] = leaf;
        }

        if (duplicateCount > 0)
        {
            cout << "duplicate reviews found: " << duplicateCount << endl;

            if (duplicateCount <= 10)
            {
                cout << "duplicate ids (renamed): ";
                for (auto& d : duplicateTracker)
                {
                    cout << d.first << " (" << d.second << "x) ";
                }
                cout << endl;
            }
        }

        cout << "building tree structure..." << endl;
        root = buildTree(leaves);

        auto end = chrono::high_resolution_clock::now();
        auto totalTime = chrono::duration_cast<chrono::milliseconds>(end - start);

        cout << "merkle tree built in " << totalTime.count() << " ms" << endl;
        cout << "root hash: " << root->hash << endl;
        cout << "unique leaf count: " << leafMap.size() << endl;
    }

    // get root hash
    string getRootHash() const
    {
        return root ? root->hash : "";
    }

    // generate membership proof
    vector<string> generateProof(const string& reviewId)
    {
        vector<string> proof;

        auto it = leafMap.find(reviewId);
        if (it == leafMap.end())
            return proof;

        auto current = it->second;
        proof.reserve(40);

        while (current != root && current->parent != nullptr)
        {
            auto parent = current->parent;

            if (parent->left.get() == current.get())
            {
                proof.push_back(parent->right->hash);
                proof.push_back("r"); //r for right
            }
            else
            {
                proof.push_back(parent->left->hash);
                proof.push_back("l"); //l for left
            }

            current = parent;
        }

        return proof;
    }

    // verify inclusion proof
    static bool verifyProof(const string& reviewData,
        const vector<string>& proof,
        const string& rootHash)
    {
        if (proof.empty()) return false;

        SHA256 hasher;
        string current = hasher(reviewData);

        for (size_t i = 0; i < proof.size(); i += 2)
        {
            if (i + 1 >= proof.size()) break;

            string siblingHash = proof[i];
            string direction = proof[i + 1];

            if (direction == "r") //right
            {
                current = (current < siblingHash)
                    ? hasher(current + siblingHash)
                    : hasher(siblingHash + current);
            }
            else
            {
                current = (siblingHash < current)
                    ? hasher(siblingHash + current)
                    : hasher(current + siblingHash);
            }
        }

        return current == rootHash;
    }

    // check if review id exists
    bool contains(const string& reviewId) const
    {
        return leafMap.find(reviewId) != leafMap.end();
    }

    // count leaves
    size_t getLeafCount() const
    {
        return leafMap.size();
    }

    //helper method to insert a leaf and return new root
    shared_ptr<MerkleNode> insertLeaf(shared_ptr<MerkleNode> currentRoot, shared_ptr<MerkleNode> newLeaf) 
    {
        if (!currentRoot) return newLeaf;

        if (currentRoot->isLeaf()) 
        {
            // create a new parent for two leaves
            string parentHash = combineHashes(currentRoot->hash, newLeaf->hash);
            auto parent = make_shared<MerkleNode>(parentHash, currentRoot, newLeaf);
            currentRoot->parent = parent;
            newLeaf->parent = parent;
            return parent;
        }

        //always insert to the right subtree, or create new parent if needed
        int leftCount = countLeaves(currentRoot->left);
        int rightCount = countLeaves(currentRoot->right);

        if (leftCount <= rightCount) {
            //insert in left subtree
            auto newLeft = insertLeaf(currentRoot->left, newLeaf);
            currentRoot->left = newLeft;
            newLeft->parent = currentRoot;
        }
        else {
            //insert in right subtree  
            auto newRight = insertLeaf(currentRoot->right, newLeaf);
            currentRoot->right = newRight;
            newRight->parent = currentRoot;
        }

        //recalculate this node's hash
        currentRoot->hash = combineHashes(currentRoot->left->hash, currentRoot->right->hash);
        return currentRoot;
    }

    //count leaves in a subtree
    int countLeaves(shared_ptr<MerkleNode> node) 
    {
        if (!node) return 0;
        if (node->isLeaf()) 
            return 1;
        return countLeaves(node->left) + countLeaves(node->right);
    }

    //add a single review to the tree (partial rebuild)
    void addReview(const string& reviewData, const string& reviewId) 
    {
        if (leafMap.find(reviewId) != leafMap.end()) 
        {
            throw invalid_argument("Review ID already exists: " + reviewId);
        }

        cout << "Adding review " << reviewId << " (partial update)" << endl;
        auto start = chrono::high_resolution_clock::now();

        //create new leaf node
        string leafHash = computeHash(reviewData);
        auto newLeaf = make_shared<MerkleNode>(leafHash, reviewId);

        //add to leaf map
        leafMap[reviewId] = newLeaf;

        if (!root) 
        {
            //tree is empty, this becomes the root
            root = newLeaf;
        }
        else 
        {
            //insert new leaf and update affected nodes
            root = insertLeaf(root, newLeaf);
        }

        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
        cout << "Review added in " << duration.count() << " ms" << endl;
        cout << "New root: " << root->hash << endl;
    }


    // print tree structure
    void printTree(int maxLevels = 3) const
    {
        if (!root)
        {
            cout << "tree is empty" << endl;
            return;
        }

        queue<shared_ptr<MerkleNode>> q;
        q.push(root);
        int level = 0;

        while (!q.empty() && level < maxLevels)
        {
            int size = q.size();
            cout << "level " << level << " (" << size << " nodes): ";

            for (int i = 0; i < size; i++)
            {
                auto node = q.front();
                q.pop();

                cout << node->hash.substr(0, 8) << "... ";

                if (node->left) q.push(node->left);
                if (node->right) q.push(node->right);
            }

            cout << endl;
            level++;
        }

        if (!q.empty())
            cout << "... (additional levels not shown)" << endl;
    }
};
