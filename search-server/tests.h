#pragma once
#include "out_operator.h"

#include <iostream>

using namespace std::string_literals;

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file,
                     const std::string& func, unsigned line, const std::string& hint) {
    if (t != u) {
        std::cout << std::boolalpha;
        std::cout << file << "("s << line << "): "s << func << ": "s;
        std::cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        std::cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            std::cout << " Hint: "s << hint;
        }
        std::cout << std::endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line,
                const std::string& hint);

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename Func>
void RunTestImpl(const Func& func, const std::string& func_name) {
    func();
    std::cerr << func_name << " OK"s << std::endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent();

//Добавление документов. Добавленный документ должен находиться по поисковому запросу, который содержит слова из документа.
void TestAddDocuments();

//Поддержка минус-слов. Документы, содержащие минус-слова из поискового запроса, не должны включаться в результаты поиска.
void TestExcludeDocumentsFromResultsByMinusWords();

//Соответствие документов поисковому запросу. При этом должны быть возвращены все слова из поискового запроса, присутствующие
//в документе. Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов.
void TestMatchDocumentsAndQuery();

//Сортировка найденных документов по релевантности. Возвращаемые при поиске документов результаты должны быть отсортированы в порядке убывания релевантности.
void TestSortDocumentsByRelevance();

//Вычисление рейтинга документов. Рейтинг добавленного документа равен среднему арифметическому оценок документа.
void TestCalculateRating();

//Фильтрация результатов поиска с использованием предиката, задаваемого пользователем.
void TestFilterResultByUserPredicate();

//Поиск документов, имеющих заданный статус.
void TestSearchDocumentsWithUserStatus();

//Корректное вычисление релевантности найденных документов.
void TestCalculateRelevance();

//Вывод результатов поиска страницами
void TestPaginateResult();

//Добавление запросов в очередь
void TestAddFindRequest();

void TestGetDocumentIdWithFor();

void TestGetWordFrequencies();

void TestRemoveDocument();

void TestRemoveDuplicates();

// --------- Окончание модульных тестов поисковой системы -----------

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer();
