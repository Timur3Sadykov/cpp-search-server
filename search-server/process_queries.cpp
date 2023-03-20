#include "process_queries.h"
#include "document.h"

#include <algorithm>
#include <execution>
#include <string_view>

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server,
                                                  const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> result(queries.size());

    transform(std::execution::par,
              queries.begin(), queries.end(),
              result.begin(),
              [&search_server](std::string_view query) { return search_server.FindTopDocuments(query); });

    return result;
}

std::list<Document> ProcessQueriesJoined(const SearchServer& search_server,
                                         const std::vector<std::string>& queries) {
    std::list<Document> result;

    for (const std::vector<Document>& docs : ProcessQueries(search_server, queries)) {
        std::move(docs.begin(), docs.end(), std::back_inserter(result));
    }

    return result;
}
