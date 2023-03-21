#include "search_server.h"

#include <cmath>

using namespace std;

SearchServer::SearchServer(string_view view_stop_words_text)
        : SearchServer(SplitIntoWordsView(view_stop_words_text)) {}

SearchServer::SearchServer(const string& string_stop_words_text)
        : SearchServer(SearchServer(string_view(string_stop_words_text))) {}

void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status, const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Недопустимый id документа"s);
    }
    const auto words = SplitIntoWordsViewNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (string_view word : words) {
        auto it_word = (dictionary_.emplace(string{word.begin(), word.end()})).first;

        word_to_document_freqs_[*it_word][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][*it_word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentStatus input_status) const {
    return FindTopDocuments(execution::seq, raw_query,
                            [input_status](int document_id, DocumentStatus status, int rating) { return status == input_status; });
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const execution::parallel_policy&,
                                                                       string_view raw_query, int document_id) const {
    if (!document_ids_.count(document_id)) {
        throw out_of_range("Недопустимый id документа MatchDocument"s);
    }

    const auto query = ParseQuery(raw_query, true);

    for (string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return {vector<string_view>{}, documents_.at(document_id).status};
        }
    }

    vector<string_view> matched_words(query.plus_words.size());
    auto it = copy_if(execution::par,
                      query.plus_words.begin(), query.plus_words.end(),
                      matched_words.begin(),
                      [this, document_id](string_view word) {
                                return word_to_document_freqs_.count(word) && word_to_document_freqs_.at(word).count(document_id);
                            });

    sort(execution::par, matched_words.begin(), it);
    auto last = unique(execution::par, matched_words.begin(), it);
    matched_words.resize(distance(matched_words.begin(), last));

    return {matched_words, documents_.at(document_id).status};
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const execution::sequenced_policy&,
                                                                       string_view raw_query, int document_id) const {
    if (!document_ids_.count(document_id)) {
        throw out_of_range("Недопустимый id документа MatchDocument"s);
    }

    const auto query = ParseQuery(raw_query);

    for (string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return {vector<string_view>{}, documents_.at(document_id).status};
        }
    }

    vector<string_view> matched_words;
    for (string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }

    return {matched_words, documents_.at(document_id).status};
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(string_view raw_query, int document_id) const {
    return MatchDocument(execution::seq, raw_query, document_id);
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const map<string_view, double> null_result;
    auto iter = document_to_word_freqs_.find(document_id);
    if (iter != document_to_word_freqs_.end()) {
        return iter->second;
    }
    return null_result;
}

void SearchServer::RemoveDocument(const execution::parallel_policy&, int document_id) {
    if (!document_ids_.count(document_id)) {
        throw out_of_range("Недопустимый id документа при удалении"s);
    }

    const auto& word_to_freqs = document_to_word_freqs_[document_id];
    vector<string_view> words;
    words.reserve(word_to_freqs.size());
    for (const auto& word_pair : word_to_freqs) {
        words.push_back(word_pair.first);
    }

    for_each(std::execution::par,
             words.begin(), words.end(),
             [this, document_id](string_view word) {
                    word_to_document_freqs_[word].erase(document_id);
                });

    document_to_word_freqs_.erase(document_id);
    documents_.erase(document_id);
    document_ids_.erase(document_id);
}

void SearchServer::RemoveDocument(const execution::sequenced_policy&, int document_id) {
    const auto iter_document_to_word_freqs_ = document_to_word_freqs_.find(document_id);

    if (iter_document_to_word_freqs_ == document_to_word_freqs_.end()) {
        throw out_of_range("Недопустимый id документа при удалении"s);
    }

    const map<string_view, double>& word_to_freqs = iter_document_to_word_freqs_->second;
    for (const auto& [word, freq] : word_to_freqs) {
        auto& document_freqs = word_to_document_freqs_.find(word)->second;
        document_freqs.erase(document_id);
    }

    document_to_word_freqs_.erase(document_id);
    documents_.erase(document_id);
    document_ids_.erase(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    RemoveDocument(execution::seq, document_id);
}

bool SearchServer::IsStopWord(string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(string_view word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
    vector<string> words;
    for (const string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Слово "s + word + " не валидно"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

vector<string_view> SearchServer::SplitIntoWordsViewNoStop(string_view text) const {
    vector<string_view> words;
    for (string_view word : SplitIntoWordsView(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Слово "s + string(word) + " не валидно"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
    if (text.empty()) {
        throw invalid_argument("Пустая строка в запросе"s);
    }
    bool is_minus = false;
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    if (text.empty() || text[0] == '-' || !IsValidWord(text)) {
        throw invalid_argument("Запрос "s + string(text) + " не вылидный");
    }

    return {text, is_minus, IsStopWord(text)};
}

SearchServer::Query SearchServer::ParseQuery(string_view text, bool is_parallel) const {
    vector<string_view> words = SplitIntoWordsView(text);
    Query query;
    query.minus_words.reserve(words.size());
    query.plus_words.reserve(words.size());

    for (string_view word : words) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.push_back(query_word.data);
            } else {
                query.plus_words.push_back(query_word.data);
            }
        }
    }

    if (is_parallel) {
        return query;
    }

    sort(query.minus_words.begin(), query.minus_words.end());
    auto last_m = unique(query.minus_words.begin(), query.minus_words.end());
    query.minus_words.resize(distance(query.minus_words.begin(), last_m));

    sort(query.plus_words.begin(), query.plus_words.end());
    auto last_p = unique(query.plus_words.begin(), query.plus_words.end());
    query.plus_words.resize(distance(query.plus_words.begin(), last_p));

    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

void AddDocument(SearchServer& search_server, int document_id, string_view document, DocumentStatus status,
                 const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    } catch (const exception& e) {
        cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, string_view raw_query) {
    cout << "Результаты поиска по запросу: "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            cout << document;
        }
    } catch (const exception& e) {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, string_view query) {
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;
        for (const int document_id : search_server) {
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    } catch (const exception& e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}
