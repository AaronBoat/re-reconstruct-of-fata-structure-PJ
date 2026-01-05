#include "mysolution.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <string>
#include <set>

using namespace std;

// Load base vectors from file (line-by-line format)
vector<float> load_base_vectors(const string &filename, int &dimension, int &num_vectors)
{
    ifstream file(filename);
    if (!file.is_open())
    {
        cerr << "Failed to open file: " << filename << endl;
        return vector<float>();
    }

    string line;
    vector<float> vectors;
    num_vectors = 0;
    dimension = 0;

    cout << "Loading vectors..." << flush;
    int progress_counter = 0;

    while (getline(file, line))
    {
        if (line.empty())
            continue;

        istringstream iss(line);
        vector<float> vec;
        float val;

        while (iss >> val)
        {
            vec.push_back(val);
        }

        if (vec.empty())
            continue;

        // Determine dimension from first vector
        if (dimension == 0)
        {
            dimension = vec.size();
            cout << " (dimension: " << dimension << ")" << endl;
        }

        // Add vector data
        for (float v : vec)
        {
            vectors.push_back(v);
        }
        num_vectors++;

        // Progress indicator
        if (++progress_counter % 100000 == 0)
        {
            cout << "  Loaded " << num_vectors << " vectors..." << endl;
        }
    }

    file.close();
    return vectors;
}

// Load query vectors from file
vector<vector<float>> load_query_vectors(const string &filename, int dimension)
{
    ifstream file(filename);
    if (!file.is_open())
    {
        cerr << "Failed to open file: " << filename << endl;
        return vector<vector<float>>();
    }

    string line;
    vector<vector<float>> queries;
    bool first_line = true;

    while (getline(file, line))
    {
        if (line.empty())
            continue;

        istringstream iss(line);
        vector<float> vec;
        float val;

        while (iss >> val)
        {
            vec.push_back(val);
        }

        if (vec.empty())
            continue;

        // Skip metadata line (e.g., "100 128")
        if (first_line && vec.size() == 2 && vec[0] > 0 && vec[1] > 0 && vec[0] < 100000 && vec[1] < 1000)
        {
            first_line = false;
            continue;
        }
        first_line = false;

        // Validate dimension
        if (dimension > 0 && vec.size() != dimension)
        {
            cerr << "Query dimension mismatch! Expected " << dimension
                 << ", got " << vec.size() << " in line" << endl;
            continue;
        }

        queries.push_back(vec);
    }

    file.close();
    return queries;
}

// Load groundtruth from file
vector<vector<int>> load_groundtruth(const string &filename)
{
    ifstream file(filename);
    if (!file.is_open())
    {
        cerr << "Failed to open file: " << filename << endl;
        return vector<vector<int>>();
    }

    string line;
    vector<vector<int>> groundtruth;
    bool first_line = true;

    while (getline(file, line))
    {
        if (line.empty())
            continue;

        istringstream iss(line);
        vector<int> vec;
        int val;

        while (iss >> val)
        {
            vec.push_back(val);
        }

        if (vec.empty())
            continue;

        // Skip metadata line if exists
        if (first_line && vec.size() == 2 && vec[0] > 0 && vec[1] > 0 && vec[0] < 100000 && vec[1] < 1000)
        {
            first_line = false;
            continue;
        }
        first_line = false;

        groundtruth.push_back(vec);
    }

    file.close();
    return groundtruth;
}

// Calculate recall@K
double calculate_recall(const vector<vector<int>> &results, const vector<vector<int>> &groundtruth, int k)
{
    if (results.size() != groundtruth.size())
    {
        cerr << "Results and groundtruth size mismatch!" << endl;
        return 0.0;
    }

    int total_recall = 0;
    for (size_t i = 0; i < results.size(); ++i)
    {
        set<int> gt_set;
        for (int j = 0; j < min(k, (int)groundtruth[i].size()); ++j)
        {
            gt_set.insert(groundtruth[i][j]);
        }

        int hits = 0;
        for (int j = 0; j < min(k, (int)results[i].size()); ++j)
        {
            if (gt_set.count(results[i][j]) > 0)
            {
                hits++;
            }
        }
        total_recall += hits;
    }

    return (double)total_recall / (results.size() * k);
}

int main(int argc, char *argv[])
{
    // Default to SIFT dataset
    // Test with GLOVE dataset (按RECONSTRUCTION_GUIDE.md要求)
    string dataset_dir = "../data_o/data_o/glove";
    bool use_cache = false;
    bool save_cache = false;
    int custom_ef_search = -1;

    if (argc > 1)
    {
        dataset_dir = argv[1];
    }

    // Parse command line arguments
    for (int i = 2; i < argc; ++i)
    {
        string arg = argv[i];
        if (arg == "--use-cache")
        {
            use_cache = true;
        }
        else if (arg == "--save-cache")
        {
            save_cache = true;
        }
        else if (arg == "--ef-search" && i + 1 < argc)
        {
            custom_ef_search = atoi(argv[i + 1]);
            ++i;
        }
    }

    string base_file = dataset_dir + "/base.txt";
    string query_file = dataset_dir + "/query.txt";
    string groundtruth_file = dataset_dir + "/groundtruth.txt";
    string cache_file = dataset_dir + "_graph_cache.bin";

    cout << "Using dataset: " << dataset_dir << endl;

    // Try to load cached graph first
    Solution solution;
    bool loaded_from_cache = false;
    int dimension = 0, num_vectors = 0;

    if (use_cache)
    {
        cout << "Attempting to load graph from cache: " << cache_file << endl;
        auto cache_start = chrono::high_resolution_clock::now();
        // Note: load_graph method needs to be implemented in Solution class
        // if (solution.load_graph(cache_file))
        // {
        //     auto cache_end = chrono::high_resolution_clock::now();
        //     auto cache_time = chrono::duration_cast<chrono::milliseconds>(cache_end - cache_start).count();
        //     cout << "✓ Graph loaded from cache in " << cache_time << " ms" << endl;
        //     loaded_from_cache = true;

        //     // Get dimension from a quick peek at base file for query loading
        //     ifstream peek(base_file);
        //     if (peek.is_open())
        //     {
        //         string line;
        //         getline(peek, line);
        //         istringstream iss(line);
        //         float val;
        //         while (iss >> val)
        //             dimension++;
        //     }
        // }
        // else
        // {
        //     cout << "✗ Failed to load cache, will build new graph..." << endl;
        // }
    }

    if (!loaded_from_cache)
    {
        cout << "Loading base vectors..." << endl;
        vector<float> base_vectors = load_base_vectors(base_file, dimension, num_vectors);

        if (base_vectors.empty())
        {
            cerr << "Failed to load base vectors" << endl;
            return 1;
        }

        cout << "Loaded " << num_vectors << " vectors of dimension " << dimension << endl;

        // Build index
        cout << "\n" << string(60, '=') << endl;
        cout << "[BUILD PHASE] Starting HNSW construction..." << endl;
        cout << "  Vectors: " << num_vectors << " x " << dimension << " dims" << endl;
        cout << "  Expected time: ~5-15 minutes" << endl;
        cout << string(60, '=') << endl;
        cout << flush;
        
        auto build_start = chrono::high_resolution_clock::now();
        solution.build(dimension, base_vectors);
        auto build_end = chrono::high_resolution_clock::now();
        auto build_time = chrono::duration_cast<chrono::milliseconds>(build_end - build_start).count();

        cout << "\n" << string(60, '=') << endl;
        cout << "[BUILD COMPLETE]" << endl;
        cout << "  Build time: " << build_time << " ms";
        cout << " (" << fixed << setprecision(2) << (build_time / 1000.0) << "s)" << endl;
        if (build_time < 2000000) {
            cout << "  Status: \u2713 PASS (< 2000s)" << endl;
        } else {
            cout << "  Status: \u2717 TIMEOUT RISK!" << endl;
        }
        cout << string(60, '=') << endl;

        // Save cache if requested
        // if (save_cache)
        // {
        //     cout << "Saving graph cache to: " << cache_file << endl;
        //     if (solution.save_graph(cache_file))
        //     {
        //         cout << "✓ Graph cache saved successfully" << endl;
        //     }
        //     else
        //     {
        //         cout << "✗ Failed to save graph cache" << endl;
        //     }
        // }
    }

    // Apply custom ef_search if specified
    // if (custom_ef_search > 0)
    // {
    //     cout << "Setting ef_search to " << custom_ef_search << endl;
    //     solution.set_ef_search(custom_ef_search);
    // }

    // Load and search queries
    cout << "\nLoading query vectors..." << endl;
    vector<vector<float>> queries = load_query_vectors(query_file, dimension);

    if (queries.empty())
    {
        cerr << "Failed to load queries" << endl;
        return 1;
    }

    cout << "Loaded " << queries.size() << " query vectors" << endl;

    // Load groundtruth
    cout << "\nLoading groundtruth..." << endl;
    vector<vector<int>> groundtruth = load_groundtruth(groundtruth_file);

    if (groundtruth.empty())
    {
        cerr << "Failed to load groundtruth (continuing without recall calculation)" << endl;
    }
    else
    {
        cout << "Loaded groundtruth for " << groundtruth.size() << " queries" << endl;
    }

    // Perform searches
    cout << "\n" << string(60, '=') << endl;
    cout << "[SEARCH PHASE] Performing " << queries.size() << " searches..." << endl;
    cout << string(60, '=') << endl;

    auto search_start = chrono::high_resolution_clock::now();

    vector<vector<int>> all_results;
    int progress_step = max(1, (int)queries.size() / 10);
    for (size_t i = 0; i < queries.size(); ++i)
    {
        if (i > 0 && i % progress_step == 0) {
            auto now = chrono::high_resolution_clock::now();
            auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - search_start).count();
            cout << "  Progress: " << (i * 100 / queries.size()) << "% (" << i << "/" << queries.size() 
                 << ") - Avg: " << fixed << setprecision(2) << (double)elapsed / i << "ms/query" << endl;
        }
        int results[10];
        solution.search(queries[i], results);

        vector<int> result_vec(results, results + 10);
        all_results.push_back(result_vec);

        if (i < 5) // Print first 5 results
        {
            cout << "Query " << i << " results: ";
            for (int j = 0; j < 10; ++j)
            {
                cout << results[j] << " ";
            }
            cout << endl;
        }
    }

    auto search_end = chrono::high_resolution_clock::now();
    auto search_time = chrono::duration_cast<chrono::milliseconds>(search_end - search_start).count();

    cout << "\n" << string(60, '=') << endl;
    cout << "[SEARCH COMPLETE]" << endl;
    cout << "  Total time: " << search_time << " ms" << endl;
    cout << "  Average time: " << fixed << setprecision(2)
         << (double)search_time / queries.size() << " ms/query";
    if (search_time / queries.size() < 2.0) {
        cout << " \u2713 Excellent";
    }
    cout << endl;
    cout << string(60, '=') << endl;

    // Calculate recall
    if (!groundtruth.empty() && groundtruth.size() == all_results.size())
    {
        cout << "\n[EVALUATION] Calculating recall..." << endl;
        double recall_1 = calculate_recall(all_results, groundtruth, 1);
        double recall_10 = calculate_recall(all_results, groundtruth, 10);

        cout << "\n" << string(60, '=') << endl;
        cout << "[FINAL RESULTS]" << endl;
        cout << string(60, '=') << endl;
        cout << "Recall@1:  " << fixed << setprecision(4) << recall_1;
        if (recall_1 >= 0.95) cout << " \u2713";
        cout << endl;
        
        cout << "Recall@10: " << fixed << setprecision(4) << recall_10;
        if (recall_10 >= 0.98) {
            cout << " \u2713 PASS" << endl;
        } else {
            cout << " \u2717 FAIL (need >= 0.98)" << endl;
            cout << "  Gap: -" << fixed << setprecision(2) << ((0.98 - recall_10) * 100) << "%" << endl;
        }
        cout << string(60, '=') << endl;
    }

    return 0;
}
