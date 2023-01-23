#include "tests.h"
#include "search_server.h"
#include "paginator.h"
#include "request_queue.h"
#include "remove_duplicates.h"

using namespace std;

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server("Test"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

//Добавление документов. Добавленный документ должен находиться по поисковому запросу, который содержит слова из документа.
void TestAddDocuments() {
    const int doc_id = 42;
    SearchServer server("Test"s);
    server.AddDocument(doc_id, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});

    const auto found_docs = server.FindTopDocuments("cat"s);
    ASSERT_EQUAL(found_docs.size(), 1u);

    const Document& doc0 = found_docs[0];
    ASSERT_EQUAL(doc0.id, doc_id);
}

//Поддержка минус-слов. Документы, содержащие минус-слова из поискового запроса, не должны включаться в результаты поиска.
void TestExcludeDocumentsFromResultsByMinusWords() {
    const int doc_id = 42;
    SearchServer server("Test"s);
    server.AddDocument(doc_id, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});

    const auto found_docs = server.FindTopDocuments("cat city"s);
    ASSERT_EQUAL(found_docs.size(), 1u);

    const Document& doc0 = found_docs[0];
    ASSERT_EQUAL(doc0.id, doc_id);

    ASSERT_HINT(server.FindTopDocuments("cat -city"s).empty(), "Minus word removes the document from the search results"s);
}

//Соответствие документов поисковому запросу. При этом должны быть возвращены все слова из поискового запроса, присутствующие
//в документе. Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов.
void TestMatchDocumentsAndQuery() {
    const int doc_id = 42;
    SearchServer server("Test"s);
    server.AddDocument(doc_id, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});

    const auto matched_words = get<0>(server.MatchDocument("cat city"s, doc_id));
    vector<string> expected_result = {"cat"s, "city"s};
    ASSERT_EQUAL_HINT(matched_words, expected_result, "Two words expected"s);

    const auto matched_words_with_minus = get<0>(server.MatchDocument("cat -city"s, doc_id));
    ASSERT_HINT(matched_words_with_minus.empty(), "Minus word removes the document from the search results"s);
}

//Сортировка найденных документов по релевантности. Возвращаемые при поиске документов результаты должны быть отсортированы в порядке убывания релевантности.
void TestSortDocumentsByRelevance() {
    SearchServer search_server("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

    const auto found_docs =  search_server.FindTopDocuments("пушистый ухоженный кот"s);
    ASSERT_EQUAL(found_docs.size(), 3u);
    ASSERT_EQUAL(found_docs[0].id, 1);
    ASSERT_EQUAL(found_docs[1].id, 0);
    ASSERT_EQUAL(found_docs[2].id, 2);
}

//Вычисление рейтинга документов. Рейтинг добавленного документа равен среднему арифметическому оценок документа.
void TestCalculateRating() {
    SearchServer server("Test"s);

    server.AddDocument(0, "cat in the city"s, DocumentStatus::ACTUAL, {10, 20, 30});
    const auto found_docs_cat = server.FindTopDocuments("cat"s);
    ASSERT_EQUAL(found_docs_cat.size(), 1u);
    ASSERT_EQUAL(found_docs_cat[0].rating, (10+20+30) / 3);

    server.AddDocument(1, "dog in the city"s, DocumentStatus::ACTUAL, {5, 10, 15});
    const auto found_docs_dog = server.FindTopDocuments("dog"s);
    ASSERT_EQUAL(found_docs_dog.size(), 1u);
    ASSERT_EQUAL(found_docs_dog[0].rating, (5 + 10 + 15) / 3);

    server.AddDocument(2, "parrot in the city"s, DocumentStatus::ACTUAL, {});
    const auto found_docs_parrot = server.FindTopDocuments("parrot"s);
    ASSERT_EQUAL(found_docs_parrot.size(), 1u);
    ASSERT_EQUAL(found_docs_parrot[0].rating, 0);
}

//Фильтрация результатов поиска с использованием предиката, задаваемого пользователем.
void TestFilterResultByUserPredicate() {
    SearchServer search_server("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

    const auto found_docs = search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
    ASSERT_EQUAL(found_docs.size(), 2u);
    ASSERT_EQUAL(found_docs[0].id, 0);
    ASSERT_EQUAL(found_docs[1].id, 2);
}

//Поиск документов, имеющих заданный статус.
void TestSearchDocumentsWithUserStatus() {
    SearchServer search_server("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

    const auto found_docs = search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED);
    ASSERT_EQUAL(found_docs.size(), 1u);
    ASSERT_EQUAL(found_docs[0].id, 3);
}

//Корректное вычисление релевантности найденных документов.
void TestCalculateRelevance() {
    SearchServer search_server("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

    const auto found_docs = search_server.FindTopDocuments("пушистый ухоженный кот"s);
    ASSERT_EQUAL(found_docs.size(), 3u);
    ASSERT(found_docs[0].relevance - 0.866434 < EPSILON);
    ASSERT(found_docs[1].relevance - 0.173287 < EPSILON);
    ASSERT(found_docs[2].relevance - 0.173287 < EPSILON);
}

void TestPaginateResult() {
    SearchServer search_server("and with"s);
    search_server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big cat nasty hair"s, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog cat Vladislav"s, DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "big dog hamster Borya"s, DocumentStatus::ACTUAL, {1, 1, 1});

    const auto search_results = search_server.FindTopDocuments("curly dog"s);
    int page_size = 2;
    const auto pages = Paginate(search_results, page_size);

    auto iterator_range = *pages.begin();
    auto document = *iterator_range.begin();
    ASSERT_EQUAL(document.id, 2);

    document = *(next(iterator_range.begin()));
    ASSERT_EQUAL(document.id, 4);

    iterator_range = *(next(pages.begin()));
    document = *iterator_range.begin();
    ASSERT_EQUAL(document.id, 5);
}

void TestAddFindRequest() {
    SearchServer search_server("and in at"s);
    RequestQueue request_queue(search_server);

    search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, {1, 1, 1});

    // 1439 запросов с нулевым результатом
    for (int i = 0; i < 1439; ++i) {
        request_queue.AddFindRequest("empty request"s);
    }
    ASSERT_EQUAL(request_queue.GetNoResultRequests(), 1439);

    // все еще 1439 запросов с нулевым результатом
    request_queue.AddFindRequest("curly dog"s);
    ASSERT_EQUAL(request_queue.GetNoResultRequests(), 1439);

    // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
    request_queue.AddFindRequest("big collar"s);
    ASSERT_EQUAL(request_queue.GetNoResultRequests(), 1438);

    // первый запрос удален, 1437 запросов с нулевым результатом
    request_queue.AddFindRequest("sparrow"s);
    ASSERT_EQUAL(request_queue.GetNoResultRequests(), 1437);
}

void TestGetDocumentIdWithFor() {
    SearchServer search_server("and in at"s);
    search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, {1, 1, 1});

    vector<int> ids;
    for (const int document_id : search_server) {
        ids.push_back(document_id);
    }

    vector<int> true_ids = {1, 2, 3, 4, 5};

    ASSERT_EQUAL(ids, true_ids);
}

void TestGetWordFrequencies() {
    SearchServer search_server("and in at"s);
    search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});

    map<string, double> word_frequencies = search_server.GetWordFrequencies(1);

    map<string, double> true_word_frequencies = {{"cat"s, 0.25}, {"curly"s, 0.5}, {"tail"s, 0.25}};

    ASSERT_EQUAL_HINT(word_frequencies, true_word_frequencies, "{cat: 0.25, curly: 0.5, tail: 0.25}"s);
}

void TestRemoveDocument() {
    SearchServer search_server("and in at"s);
    search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "big dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big dog fancy collar "s, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, {1, 1, 1});

    search_server.RemoveDocument(1);

    vector<int> ids;
    for (const int document_id : search_server) {
        ids.push_back(document_id);
    }
    vector<int> true_ids = {2, 3, 4, 5};
    ASSERT_EQUAL(ids, true_ids);

    const auto found_docs = search_server.FindTopDocuments("cat"s);
    ASSERT_EQUAL(found_docs.size(), 0u);
}

void TestRemoveDuplicates() {
    SearchServer search_server("and in at"s);
    search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "big dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big dog fancy collar collar collar"s, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, {1, 1, 1});

    RemoveDuplicates(search_server);

    ASSERT_EQUAL(search_server.GetDocumentCount(), 4);

    const auto found_docs = search_server.FindTopDocuments("dog"s);
    ASSERT_EQUAL(found_docs.size(), 3u);
    ASSERT_EQUAL(found_docs[0].id, 2);
    ASSERT_EQUAL(found_docs[1].id, 4);
    ASSERT_EQUAL(found_docs[2].id, 5);
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddDocuments);
    RUN_TEST(TestExcludeDocumentsFromResultsByMinusWords);
    RUN_TEST(TestMatchDocumentsAndQuery);
    RUN_TEST(TestSortDocumentsByRelevance);
    RUN_TEST(TestCalculateRating);
    RUN_TEST(TestFilterResultByUserPredicate);
    RUN_TEST(TestSearchDocumentsWithUserStatus);
    RUN_TEST(TestCalculateRelevance);
    RUN_TEST(TestPaginateResult);
    RUN_TEST(TestAddFindRequest);
    RUN_TEST(TestGetDocumentIdWithFor);
    RUN_TEST(TestGetWordFrequencies);
    RUN_TEST(TestRemoveDocument);
    RUN_TEST(TestRemoveDuplicates);
}
