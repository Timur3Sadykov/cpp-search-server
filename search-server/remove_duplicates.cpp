#include "remove_duplicates.h"

#include <string>
#include <vector>
#include <map>
#include <set>

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    map<set<string>, vector<int>> comparison_map;

    for (const int document_id : search_server) {
        set<string> words;
        for (const auto& [word, freq] : search_server.GetWordFrequencies(document_id)) {
            words.insert(word);
        }
        comparison_map[words].push_back(document_id);
    }

    for (auto& [words_set, vector_ids] : comparison_map) {
        sort(vector_ids.begin(), vector_ids.end());
        while (vector_ids.size() > 1) {
            int id = vector_ids.back();
            cout << "Found duplicate document id " << id << endl;
            search_server.RemoveDocument(id);
            vector_ids.pop_back();
        }
    }
}
