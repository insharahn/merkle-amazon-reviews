# merkle-amazon-reviews
A Merkle Tree-based Amazon review integrity system.

## Modules
1. DataPreprocessor.h - Loads and cleans Amazon review JSON files. Creates unique IDs for each review, removes duplicates and empty entries.
2. MerkleTree.h - Core cryptographic engine. Builds binary hash tree from reviews using SHA-256. Each leaf = hashed review, parent nodes = hashes of children. Root hash = unique fingerprint of entire dataset. Provides proof generation/verification.
3. IntegrityVerifier.h - Trust anchor. Stores trusted root hashes, compares them to detect tampering. Saves roots to file for audit trail.
4. ExistenceProver.h - Proof generator. Creates cryptographic proofs that specific reviews exist in the tree. Provides O(log n) verification without revealing entire dataset.
5. TamperingDetector.h - Security monitor. Simulates attacks (modification, deletion, injection) and detects them via root hash changes. Provides detailed tamper reports.
6. PerformanceMeasurer.h - Benchmark system. Tests speed and memory usage. Validates project requirements (<100ms verification, handles 1M+ records).
7. CLI.h - User interface. Menu-driven system to run all functions. Load data, build tree, generate proofs, detect tampering, run tests.

The separate tests.cpp file is a Google Test suite that verifies that all functionality works correctly with real data.

## Datasets
- This project uses Amazon Product Data (Jianmo Ni) from https://nijianmo.github.io/amazon/index.html
- Download any dataset from the _"Small" subsets for experimentation_ section, unzip it, and it will work with the code
