#include <iostream>
#include <rag/index/corpus.hpp>

int main() {
    auto loaded =
        rag::index::Corpus::load(
            "C:/Users/hp/rag-cpp/enterprise_eval.ragdb");

    if (!loaded) {
        std::cerr << "Failed to load corpus: "
                  << loaded.error().message << "\n";
        return 1;
    }

    auto corpus = std::move(*loaded);

    const std::string uri =
        "uploads/secret_finance.txt";

    auto doc_id = corpus.find_by_uri(uri);

    if (!doc_id) {
        std::cerr << "Document NOT FOUND: "
                  << uri << "\n";
        return 1;
    }

    const auto* doc =
        corpus.document(*doc_id);

    if (!doc) {
        std::cerr << "Document pointer is null\n";
        return 1;
    }

    std::cout << "URI: " << doc->uri << "\n";
    std::cout << "Title: " << doc->title << "\n";
    std::cout << "Metadata:\n";

    for (const auto& [key, value] : doc->meta) {
        std::cout << "  "
                  << key
                  << " = "
                  << value
                  << "\n";
    }

    return 0;
}
