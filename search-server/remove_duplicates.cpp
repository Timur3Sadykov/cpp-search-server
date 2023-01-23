#include "remove_duplicates.h"

#include <string>
#include <vector>
#include <set>

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    set<set<string>> comparison_set;
    vector<int> duplicate_documents_ids;

    for (const int document_id : search_server) {
        set<string> words;
        for (const auto& [word, freq] : search_server.GetWordFrequencies(document_id)) {
            words.insert(word);
        }

        if (comparison_set.count(words)) {
            duplicate_documents_ids.push_back(document_id);
        }
        else {
            comparison_set.insert(words);
        }
    }

    for (int duplicate_document_id : duplicate_documents_ids) {
        cout << "Found duplicate document id "s << duplicate_document_id << endl;
        search_server.RemoveDocument(duplicate_document_id);
    }
}
