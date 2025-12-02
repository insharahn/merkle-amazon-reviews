#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <cctype>
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

//helper class for extracting reviews from json file
struct Review
{
    string reviewID, reviewerID, reviewText, summary;
    string asin;  //product ID in json file
    double overall;
    string unixReviewTime;

    //methods

    //create review
    Review(const json& j)
    {
        //handle numeric fields that might be numbers in JSON
        if (j.contains("unixReviewTime")) 
        {
            if (j["unixReviewTime"].is_number()) 
            {
                unixReviewTime = to_string(j["unixReviewTime"].get<long>());
            }
            else 
                unixReviewTime = j.value("unixReviewTime", "");
        }
        else 
        {
            unixReviewTime = ""; //just leave blank
        }

        if (j.contains("overall")) 
        {
            if (j["overall"].is_number()) 
            {
                overall = j["overall"].get<double>();
            }
            else 
            {
                overall = 0.0;
            }
        }
        else 
        {
            overall = 0.0;
        }

        // handle string fields safely with trimming
        if (j.contains("asin"))
            asin = trimString(j["asin"].get<string>());
        else
            asin = "";

        if (j.contains("reviewerID")) 
            reviewerID = trimString(j["reviewerID"].get<string>());
        else 
            reviewerID = "";

        if (j.contains("reviewText")) 
            reviewText = trimString(j["reviewText"].get<string>());
        else 
            reviewText = "";

        if (j.contains("summary")) 
            summary = trimString(j["summary"].get<string>());
        else 
            summary = "";


        //generate unique ID
        reviewID = reviewerID + "_" + asin + "_" + unixReviewTime;
    }

    //helpers
    //convert to string so review can be hashed
    string convertToString() const
    {
        ostringstream oss;
        oss << "reviewID: " << reviewID << "\n"
            << "asin: " << asin << "\n"
            << "reviewerID: " << reviewerID << "\n"
            << "reviewText: " << reviewText << "\n"
            << "summary: " << summary << "\n"
            << "overall: " << overall << "\n"
            << "unixReviewTime: " << unixReviewTime;
        return oss.str();
    }

    //trim whitespace from strings
    static string trimString(const string& str) 
    {
        size_t start = str.find_first_not_of(" \t\n\r");
        size_t end = str.find_last_not_of(" \t\n\r");

        if (start == string::npos || end == string::npos)
            return "";

        return str.substr(start, end - start + 1);
    }

    //getters
    string getUniqueID() const { return reviewID; }
};

//main data processing class
class DataPreprocessor
{
private:
    vector<Review> reviews;
    int totalRecords;

public:
    DataPreprocessor() : totalRecords(0) {}

    //load reviews from JSON file and clean
    bool loadFromJSON(const string& filename, int maxRecords)
    {
        ifstream file(filename);
        if (!file.is_open())
        {
            cout << "Could not open file " << filename << endl;
            return false;
        }

        reviews.clear(); //clear all reviews in case before loading
        string line;
        int count = 0;
        int duplicatesRemoved = 0;
        unordered_set<string> parsedIDs; //track duplicates

        cout << "Loading reviews from " << filename << endl;

        while (getline(file, line))
        {
            //skip empty lines
            if (line.empty() || isWhitespace(line))
            {
                continue;
            }

            try
            {
                json j = json::parse(line); //parse json file
                Review review(j); //construct review

                //skip reviews with empty text
                if (review.reviewText.empty())
                {
                    continue;
                }

                //check for duplicates
                string uniqueID = review.getUniqueID();
                if (parsedIDs.find(uniqueID) != parsedIDs.end()) //check if id is already in set
                {
                    duplicatesRemoved++; //dup found
                    continue; //skip duplicate
                }
                parsedIDs.insert(uniqueID);

                reviews.push_back(review); //add to reviews vector
                count++;

                if (count % 100000 == 0) //log at every 100000 reviews
                {
                    cout << "Loaded " << count << " reviews..." << endl;
                }

                if (maxRecords > 0 && count >= maxRecords)
                {
                    break;
                }
            }
            catch (const exception& e)
            {
                cout << "Error parsing JSON line: " << e.what() << endl;
                continue;
            }
        }

        totalRecords = count;
        file.close();

        if (duplicatesRemoved > 0)
        {
            cout << "Removed " << duplicatesRemoved << " duplicate reviews" << endl;
        }

        cout << "Successfully loaded " << totalRecords << " reviews" << endl;
        return true;
    }

    //helper to print sample reviews
    void printSampleReviews(int count) const
    {
        cout << "Reviews (" << min(count, totalRecords) << " of " << totalRecords << ")\n";
        for (int i = 0; i < min(count, totalRecords); i++)
        {
            const Review& r = reviews[i];
            cout << "Review " << (i + 1) << ":\n"
                << "  ID: " << r.getUniqueID() << "\n"
                << "  Product: " << r.asin << "\n"
                << "  Reviewer: " << r.reviewerID << "\n"
                << "  Rating: " << r.overall << "\n"
                << "  Summary: " << (r.summary.length() > 50 ? r.summary.substr(0, 50) + "..." : r.summary)
                << "\n" << endl;
        }
    }

    //clear all loaded data
    void clear()
    {
        reviews.clear();
        totalRecords = 0;
    }

    //helper to check if string is only whitespace
    bool isWhitespace(const string& str) const
    {
        return all_of(str.begin(), str.end(), [](unsigned char c) {
            return isspace(c);
            });
    }

    //getters
    const vector<Review>& getReviews() const { return reviews; }

    // total number of records loaded
    int getTotalRecords() const { return totalRecords; }
   
};