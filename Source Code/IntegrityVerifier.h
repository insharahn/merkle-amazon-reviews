#pragma once
#include <iostream>
#include <string>
#include <fstream>
#include <unordered_map>

using namespace std;

class IntegrityVerifier
{
private:
    unordered_map<string, string> storedRoots;
    string currentRoot;
    string currentDataset;

public:
    IntegrityVerifier() : currentRoot(""), currentDataset("") {}

    bool storeRootHash(const string& datasetName, const string& rootHash)
    {
        if (datasetName.empty() || rootHash.empty())
        {
            cout << "Error: Dataset name and root hash cannot be empty" << endl;
            return false;
        }

        storedRoots[datasetName] = rootHash;
        currentRoot = rootHash;
        currentDataset = datasetName;

        cout << "Stored root hash for dataset: " << datasetName << endl;
        cout << "Root: " << rootHash << endl;
        return true;
    }

    bool saveRootToFile(const string& filename, const string& datasetName, const string& rootHash)
    {
        ofstream file(filename, ios::app);
        if (!file.is_open())
        {
            cout << "Error: Could not open file " << filename << endl;
            return false;
        }

        file << datasetName << "|" << rootHash << "|" << time(nullptr) << endl;
        file.close();
        cout << "Root hash saved to file: " << filename << endl;
        return true;
    }

    bool loadRootsFromFile(const string& filename)
    {
        ifstream file(filename);
        if (!file.is_open())
        {
            cout << "Error: Could not open file " << filename << endl;
            return false;
        }

        storedRoots.clear();
        string line;
        int count = 0;

        while (getline(file, line))
        {
            size_t pos1 = line.find('|');
            size_t pos2 = line.find('|', pos1 + 1);

            if (pos1 != string::npos && pos2 != string::npos)
            {
                string name = line.substr(0, pos1);
                string hash = line.substr(pos1 + 1, pos2 - pos1 - 1);
                storedRoots[name] = hash;
                count++;
            }
        }

        file.close();
        cout << "Loaded " << count << " root hashes from " << filename << endl;
        return true;
    }

    string compareWithStored(const string& datasetName, const string& currentRootHash = "") {
        auto it = storedRoots.find(datasetName);
        if (it == storedRoots.end()) {
            return "NOT_FOUND: No stored root hash for dataset: " + datasetName;
        }

        string rootToCompare = currentRootHash.empty() ? currentRoot : currentRootHash;

        if (rootToCompare.empty()) {
            return "ERROR: No current root hash set";
        }

        if (rootToCompare == it->second) {
            return "INTEGRITY_VERIFIED: Dataset integrity confirmed";
        }
        else {
            return "INTEGRITY_VIOLATED: Dataset has been tampered with";
        }
    }

    static string compareRoots(const string& root1, const string& root2)
    {
        if (root1.empty() || root2.empty())
        {
            return "ERROR: One or both root hashes are empty";
        }

        if (root1 == root2)
        {
            return "ROOTS_MATCH: Datasets are identical";
        }
        else
        {
            return "ROOTS_DIFFER: Datasets are different";
        }
    }

    string detectUpdates(const string& datasetName, const string& newRoot)
    {
        auto it = storedRoots.find(datasetName);
        if (it == storedRoots.end())
        {
            return "UNKNOWN: No previous version found for dataset: " + datasetName;
        }

        if (newRoot == it->second)
        {
            return "NO_UPDATES: Dataset unchanged since last verification";
        }
        else
        {
            return "UPDATED_DETECTED: Dataset has been modified since last verification";
        }
    }

    void listStoredRoots() const
    {
        if (storedRoots.empty())
        {
            cout << "No root hashes stored" << endl;
            return;
        }

        cout << "Stored Root Hashes:" << endl;
        cout << "===================" << endl;
        for (const auto& pair : storedRoots)
        {
            cout << "Dataset: " << pair.first << endl;
            cout << "Root: " << pair.second << endl;
            cout << "-------------------" << endl;
        }
    }

    string getCurrentRoot() const { return currentRoot; }
    string getCurrentDataset() const { return currentDataset; }

    bool hasStoredRoot(const string& datasetName) const
    {
        return storedRoots.find(datasetName) != storedRoots.end();
    }

    void clear()
    {
        storedRoots.clear();
        currentRoot = "";
        currentDataset = "";
    }
};