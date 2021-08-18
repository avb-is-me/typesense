#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <collection_manager.h>
#include "collection.h"

class CollectionSpecificTest : public ::testing::Test {
protected:
    Store *store;
    CollectionManager & collectionManager = CollectionManager::get_instance();

    std::vector<std::string> query_fields;
    std::vector<sort_by> sort_fields;

    void setupCollection() {
        std::string state_dir_path = "/tmp/typesense_test/collection_specific";
        LOG(INFO) << "Truncating and creating: " << state_dir_path;
        system(("rm -rf "+state_dir_path+" && mkdir -p "+state_dir_path).c_str());

        store = new Store(state_dir_path);
        collectionManager.init(store, 1.0, "auth_key");
        collectionManager.load(8, 1000);
    }

    virtual void SetUp() {
        setupCollection();
    }

    virtual void TearDown() {
        collectionManager.dispose();
        delete store;
    }
};

TEST_F(CollectionSpecificTest, SearchTextWithHyphen) {
    std::vector<field> fields = {field("title", field_types::STRING, false),
                                 field("points", field_types::INT32, false),};

    Collection* coll1 = collectionManager.create_collection("coll1", 1, fields, "points").get();

    nlohmann::json doc;
    doc["id"] = "0";
    doc["title"] = "open-access-may-become-mandatory-for-nih-funded-research";
    doc["points"] = 100;

    ASSERT_TRUE(coll1->add(doc.dump()).ok());

    auto results = coll1->search("open-access-may-become-mandatory-for-nih-funded-research",
                                 {"title"}, "", {}, {}, {0}, 3, 1, FREQUENCY, {true}, 5).get();

    ASSERT_EQ(1, results["found"].get<size_t>());
    ASSERT_EQ(1, results["hits"].size());

    ASSERT_STREQ("0", results["hits"][0]["document"]["id"].get<std::string>().c_str());
    collectionManager.drop_collection("coll1");
}

TEST_F(CollectionSpecificTest, ExplicitHighlightFieldsConfig) {
    std::vector<field> fields = {field("title", field_types::STRING, false),
                                 field("description", field_types::STRING, false),
                                 field("author", field_types::STRING, false),
                                 field("points", field_types::INT32, false),};

    Collection* coll1 = collectionManager.create_collection("coll1", 1, fields, "points").get();

    nlohmann::json doc;
    doc["id"] = "0";
    doc["title"] = "The quick brown fox was too fast.";
    doc["description"] = "A story about a brown fox who was fast.";
    doc["author"] = "David Pernell";
    doc["points"] = 100;

    ASSERT_TRUE(coll1->add(doc.dump()).ok());

    auto results = coll1->search("brown fox pernell", {"title"}, "", {}, {}, {2}, 10,
                                 1, FREQUENCY, {false}, 1, spp::sparse_hash_set<std::string>(),
                                 spp::sparse_hash_set<std::string>(), 10, "", 30, 4, "", 1, {}, {}, {}, 0,
                                 "<mark>", "</mark>", {1}, 10000, true, false, true, "description,author").get();

    ASSERT_EQ(1, results["found"].get<size_t>());
    ASSERT_EQ(1, results["hits"].size());

    ASSERT_EQ("0", results["hits"][0]["document"]["id"].get<std::string>());
    ASSERT_EQ(2, results["hits"][0]["highlights"].size());

    ASSERT_EQ("description", results["hits"][0]["highlights"][0]["field"].get<std::string>());
    ASSERT_EQ("A story about a <mark>brown</mark> <mark>fox</mark> who was fast.", results["hits"][0]["highlights"][0]["snippet"].get<std::string>());

    ASSERT_EQ("author", results["hits"][0]["highlights"][1]["field"].get<std::string>());
    ASSERT_EQ("David <mark>Pernell</mark>", results["hits"][0]["highlights"][1]["snippet"].get<std::string>());

    // excluded fields are NOT respected if explicit highlight fields are provided

    results = coll1->search("brown fox pernell", {"title"}, "", {}, {}, {2}, 10,
                            1, FREQUENCY, {false}, 1, spp::sparse_hash_set<std::string>(),
                            {"description"}, 10, "", 30, 4, "", 1, {}, {}, {}, 0,
                            "<mark>", "</mark>", {1}, 10000, true, false, true, "description,author").get();

    ASSERT_EQ(1, results["found"].get<size_t>());
    ASSERT_EQ(1, results["hits"].size());

    ASSERT_EQ("0", results["hits"][0]["document"]["id"].get<std::string>());
    ASSERT_EQ(2, results["hits"][0]["highlights"].size());
    ASSERT_FALSE(results["hits"][0]["document"].contains("description"));

    ASSERT_EQ("description", results["hits"][0]["highlights"][0]["field"].get<std::string>());
    ASSERT_EQ("author", results["hits"][0]["highlights"][1]["field"].get<std::string>());

    // query not matching field selected for highlighting

    results = coll1->search("pernell", {"title", "author"}, "", {}, {}, {2}, 10,
                            1, FREQUENCY, {false}, 1, spp::sparse_hash_set<std::string>(),
                            {"description"}, 10, "", 30, 4, "", 1, {}, {}, {}, 0,
                            "<mark>", "</mark>", {1,1}, 10000, true, false, true, "description").get();

    ASSERT_EQ(1, results["found"].get<size_t>());
    ASSERT_EQ(1, results["hits"].size());
    ASSERT_EQ(0, results["hits"][0]["highlights"].size());

    // wildcard query with search field names

    results = coll1->search("*", {"title", "author"}, "", {}, {}, {2}, 10,
                            1, FREQUENCY, {false}, 1, spp::sparse_hash_set<std::string>(),
                            {"description"}, 10, "", 30, 4, "", 1, {}, {}, {}, 0,
                            "<mark>", "</mark>", {1,1}, 10000, true, false, true, "description,author").get();

    ASSERT_EQ(1, results["found"].get<size_t>());
    ASSERT_EQ(1, results["hits"].size());
    ASSERT_EQ(0, results["hits"][0]["highlights"].size());

    // wildcard query without search field names

    results = coll1->search("*", {}, "", {}, {}, {2}, 10,
                            1, FREQUENCY, {false}, 1, spp::sparse_hash_set<std::string>(),
                            {"description"}, 10, "", 30, 4, "", 1, {}, {}, {}, 0,
                            "<mark>", "</mark>", {1,1}, 10000, true, false, true, "description,author").get();

    ASSERT_EQ(1, results["found"].get<size_t>());
    ASSERT_EQ(1, results["hits"].size());
    ASSERT_EQ(0, results["hits"][0]["highlights"].size());

    collectionManager.drop_collection("coll1");
}

TEST_F(CollectionSpecificTest, ExactSingleFieldMatch) {
    std::vector<field> fields = {field("title", field_types::STRING, false),
                                 field("description", field_types::STRING, false),
                                 field("points", field_types::INT32, false),};

    Collection* coll1 = collectionManager.create_collection("coll1", 1, fields, "points").get();

    nlohmann::json doc1;
    doc1["id"] = "0";
    doc1["title"] = "Fast Electric Charger";
    doc1["description"] = "A product you should buy.";
    doc1["points"] = 100;

    nlohmann::json doc2;
    doc2["id"] = "1";
    doc2["title"] = "Omega Chargex";
    doc2["description"] = "Chargex is a great product.";
    doc2["points"] = 200;

    ASSERT_TRUE(coll1->add(doc1.dump()).ok());
    ASSERT_TRUE(coll1->add(doc2.dump()).ok());

    auto results = coll1->search("charger", {"title", "description"}, "", {}, {}, {2}, 10,
                                 1, FREQUENCY, {true, true}).get();

    ASSERT_EQ("0", results["hits"][0]["document"]["id"].get<std::string>());
    ASSERT_EQ("1", results["hits"][1]["document"]["id"].get<std::string>());

    collectionManager.drop_collection("coll1");
}

TEST_F(CollectionSpecificTest, OrderMultiFieldFuzzyMatch) {
    std::vector<field> fields = {field("title", field_types::STRING, false),
                                 field("description", field_types::STRING, false),
                                 field("points", field_types::INT32, false),};

    Collection* coll1 = collectionManager.create_collection("coll1", 1, fields, "points").get();

    nlohmann::json doc1;
    doc1["id"] = "0";
    doc1["title"] = "Moto Insta Share";
    doc1["description"] = "Share information with this device.";
    doc1["points"] = 100;

    nlohmann::json doc2;
    doc2["id"] = "1";
    doc2["title"] = "Portable USB Store";
    doc2["description"] = "Use it to charge your phone.";
    doc2["points"] = 50;

    ASSERT_TRUE(coll1->add(doc1.dump()).ok());
    ASSERT_TRUE(coll1->add(doc2.dump()).ok());

    auto results = coll1->search("charger", {"title", "description"}, "", {}, {}, {2}, 10,
                                 1, FREQUENCY, {true, true},
                                 10, spp::sparse_hash_set<std::string>(),
                                 spp::sparse_hash_set<std::string>(), 10, "", 30, 4, "", 40, {}, {}, {}, 0,
                                 "<mark>", "</mark>", {1, 1}).get();

    ASSERT_EQ("1", results["hits"][0]["document"]["id"].get<std::string>());
    ASSERT_EQ("0", results["hits"][1]["document"]["id"].get<std::string>());

    results = coll1->search("charger", {"title", "description"}, "", {}, {}, {2}, 10,
                            1, FREQUENCY, {true, true},
                            10, spp::sparse_hash_set<std::string>(),
                            spp::sparse_hash_set<std::string>(), 10, "", 30, 4, "", 40, {}, {}, {}, 0,
                            "<mark>", "</mark>", {2, 1}).get();

    ASSERT_EQ("1", results["hits"][0]["document"]["id"].get<std::string>());
    ASSERT_EQ("0", results["hits"][1]["document"]["id"].get<std::string>());

    // use extreme weights to push title matching ahead

    results = coll1->search("charger", {"title", "description"}, "", {}, {}, {2}, 10,
                            1, FREQUENCY, {true, true},
                            10, spp::sparse_hash_set<std::string>(),
                            spp::sparse_hash_set<std::string>(), 10, "", 30, 4, "", 40, {}, {}, {}, 0,
                            "<mark>", "</mark>", {10, 1}).get();

    ASSERT_EQ("0", results["hits"][0]["document"]["id"].get<std::string>());
    ASSERT_EQ("1", results["hits"][1]["document"]["id"].get<std::string>());

    collectionManager.drop_collection("coll1");
}

TEST_F(CollectionSpecificTest, FieldWeighting) {
    std::vector<field> fields = {field("title", field_types::STRING, false),
                                 field("description", field_types::STRING, false),
                                 field("points", field_types::INT32, false),};

    Collection* coll1 = collectionManager.create_collection("coll1", 1, fields, "points").get();

    nlohmann::json doc1;
    doc1["id"] = "0";
    doc1["title"] = "The Quick Brown Fox";
    doc1["description"] = "Share information with this device.";
    doc1["points"] = 100;

    nlohmann::json doc2;
    doc2["id"] = "1";
    doc2["title"] = "Random Title";
    doc2["description"] = "The Quick Brown Fox";
    doc2["points"] = 50;

    ASSERT_TRUE(coll1->add(doc1.dump()).ok());
    ASSERT_TRUE(coll1->add(doc2.dump()).ok());

    auto results = coll1->search("brown fox", {"title", "description"}, "", {}, {}, {2}, 10,
                                 1, FREQUENCY, {true, true},
                                 10, spp::sparse_hash_set<std::string>(),
                                 spp::sparse_hash_set<std::string>(), 10, "", 30, 4, "", 40, {}, {}, {}, 0,
                                 "<mark>", "</mark>", {1, 4}).get();

    ASSERT_EQ("1", results["hits"][0]["document"]["id"].get<std::string>());
    ASSERT_EQ("0", results["hits"][1]["document"]["id"].get<std::string>());

    collectionManager.drop_collection("coll1");
}

TEST_F(CollectionSpecificTest, MultiFieldArrayRepeatingTokens) {
    std::vector<field> fields = {field("title", field_types::STRING, false),
                                 field("description", field_types::STRING, false),
                                 field("attrs", field_types::STRING_ARRAY, false),
                                 field("points", field_types::INT32, false),};

    Collection* coll1 = collectionManager.create_collection("coll1", 1, fields, "points").get();

    nlohmann::json doc1;
    doc1["id"] = "0";
    doc1["title"] = "E182-72/4";
    doc1["description"] = "Nexsan Technologies 18 SAN Array - 18 x HDD Supported - 18 x HDD Installed";
    doc1["attrs"] = {"Hard Drives Supported > 18", "Hard Drives Installed > 18", "SSD Supported > 18"};
    doc1["points"] = 100;

    nlohmann::json doc2;
    doc2["id"] = "1";
    doc2["title"] = "RV345-K9-NA";
    doc2["description"] = "Cisco RV345P Router - 18 Ports";
    doc2["attrs"] = {"Number of Ports > 18", "Product Type > Router"};
    doc2["points"] = 50;

    ASSERT_TRUE(coll1->add(doc1.dump()).ok());
    ASSERT_TRUE(coll1->add(doc2.dump()).ok());

    auto results = coll1->search("rv345 cisco 18", {"title", "description", "attrs"}, "", {}, {}, {1}, 10,
                                 1, FREQUENCY, {true, true, true}).get();

    ASSERT_EQ("1", results["hits"][0]["document"]["id"].get<std::string>());
    ASSERT_EQ("0", results["hits"][1]["document"]["id"].get<std::string>());

    collectionManager.drop_collection("coll1");
}

TEST_F(CollectionSpecificTest, ExactMatchOnPrefix) {
    std::vector<field> fields = {field("title", field_types::STRING, false),
                                 field("points", field_types::INT32, false),};

    Collection* coll1 = collectionManager.create_collection("coll1", 1, fields, "points").get();

    nlohmann::json doc1;
    doc1["id"] = "0";
    doc1["title"] = "Yeshivah Gedolah High School";
    doc1["points"] = 100;

    nlohmann::json doc2;
    doc2["id"] = "1";
    doc2["title"] = "GED";
    doc2["points"] = 50;

    ASSERT_TRUE(coll1->add(doc1.dump()).ok());
    ASSERT_TRUE(coll1->add(doc2.dump()).ok());

    auto results = coll1->search("ged", {"title"}, "", {}, {}, {2}, 10,
                                 1, FREQUENCY, {true}, Index::DROP_TOKENS_THRESHOLD,
                                 spp::sparse_hash_set<std::string>(),
                                 spp::sparse_hash_set<std::string>(), 10, "", 30, 5,
                                 "", 1).get();

    ASSERT_EQ(2, results["hits"].size());

    ASSERT_EQ("1", results["hits"][0]["document"]["id"].get<std::string>());
    ASSERT_EQ("0", results["hits"][1]["document"]["id"].get<std::string>());

    collectionManager.drop_collection("coll1");
}

TEST_F(CollectionSpecificTest, TypoPrefixSearchWithoutPrefixEnabled) {
    std::vector<field> fields = {field("title", field_types::STRING, false),
                                 field("points", field_types::INT32, false),};

    Collection* coll1 = collectionManager.create_collection("coll1", 1, fields, "points").get();

    nlohmann::json doc1;
    doc1["id"] = "0";
    doc1["title"] = "Cisco SG25026HP Gigabit Smart Switch";
    doc1["points"] = 100;

    ASSERT_TRUE(coll1->add(doc1.dump()).ok());

    auto results = coll1->search("SG25026H", {"title"}, "", {}, {}, {2}, 10,
                                 1, FREQUENCY, {false}, 0,
                                 spp::sparse_hash_set<std::string>(),
                                 spp::sparse_hash_set<std::string>(), 10, "", 30, 5,
                                 "", 1).get();

    ASSERT_EQ(1, results["hits"].size());
    ASSERT_EQ("0", results["hits"][0]["document"]["id"].get<std::string>());

    collectionManager.drop_collection("coll1");
}

TEST_F(CollectionSpecificTest, PrefixWithTypos) {
    std::vector<field> fields = {field("title", field_types::STRING, false),
                                 field("points", field_types::INT32, false),};

    Collection* coll1 = collectionManager.create_collection("coll1", 1, fields, "points").get();

    nlohmann::json doc1;
    doc1["id"] = "0";
    doc1["title"] = "PRÍNCIPE - Restaurante e Snack Bar";
    doc1["points"] = 100;

    ASSERT_TRUE(coll1->add(doc1.dump()).ok());

    auto results = coll1->search("maria", {"title"}, "", {}, {}, {2}, 10,
                                 1, FREQUENCY, {true}).get();

    ASSERT_EQ(0, results["hits"].size());

    results = coll1->search("maria", {"title"}, "", {}, {}, {2}, 10,
                            1, FREQUENCY, {false}).get();

    ASSERT_EQ(0, results["hits"].size());

    collectionManager.drop_collection("coll1");
}

TEST_F(CollectionSpecificTest, PrefixVsExactMatch) {
    Collection *coll1;

    std::vector<field> fields = {field("title", field_types::STRING, false),
                                 field("points", field_types::INT32, false),};

    coll1 = collectionManager.get_collection("coll1").get();
    if(coll1 == nullptr) {
        coll1 = collectionManager.create_collection("coll1", 1, fields, "points").get();
    }

    std::vector<std::vector<std::string>> records = {
        {"Equivalent Ratios"},
        {"Simplifying Ratios 1"},
        {"Rational and Irrational Numbers"},
        {"Simplifying Ratios 2"},
    };

    for(size_t i=0; i<records.size(); i++) {
        nlohmann::json doc;

        doc["id"] = std::to_string(i);
        doc["title"] = records[i][0];
        doc["points"] = i;

        ASSERT_TRUE(coll1->add(doc.dump()).ok());
    }

    auto results = coll1->search("ration",
                                 {"title"}, "", {}, {}, {1}, 10, 1, FREQUENCY, {true}, 10,
                                 spp::sparse_hash_set<std::string>(),
                                 spp::sparse_hash_set<std::string>(), 10, "", 30, 5,
                                 "", 10).get();

    ASSERT_EQ(4, results["found"].get<size_t>());
    ASSERT_EQ(4, results["hits"].size());

    ASSERT_STREQ("2", results["hits"][0]["document"]["id"].get<std::string>().c_str());
    ASSERT_STREQ("3", results["hits"][1]["document"]["id"].get<std::string>().c_str());
    ASSERT_STREQ("1", results["hits"][2]["document"]["id"].get<std::string>().c_str());
    ASSERT_STREQ("0", results["hits"][3]["document"]["id"].get<std::string>().c_str());

    collectionManager.drop_collection("coll1");
}

TEST_F(CollectionSpecificTest, PrefixWithTypos2) {
    std::vector<field> fields = {field("title", field_types::STRING, false),
                                 field("points", field_types::INT32, false),};

    Collection* coll1 = collectionManager.create_collection("coll1", 1, fields, "points").get();

    nlohmann::json doc1;
    doc1["id"] = "0";
    doc1["title"] = "Av. Mal. Humberto Delgado 206, 4760-012 Vila Nova de Famalicão, Portugal";
    doc1["points"] = 100;

    ASSERT_TRUE(coll1->add(doc1.dump()).ok());

    auto results = coll1->search("maria", {"title"}, "", {}, {}, {2}, 10,
                                 1, FREQUENCY, {true}).get();

    ASSERT_EQ(0, results["hits"].size());

    results = coll1->search("maria", {"title"}, "", {}, {}, {2}, 10,
                            1, FREQUENCY, {false}).get();

    ASSERT_EQ(0, results["hits"].size());

    collectionManager.drop_collection("coll1");
}

TEST_F(CollectionSpecificTest, ImportDocumentWithIntegerID) {
    std::vector<field> fields = {field("title", field_types::STRING, false),
                                 field("points", field_types::INT32, false),};

    Collection* coll1 = collectionManager.create_collection("coll1", 1, fields, "points").get();

    nlohmann::json doc1;
    doc1["id"] = 100;
    doc1["title"] = "East India House on Wednesday evening";
    doc1["points"] = 100;

    auto add_op = coll1->add(doc1.dump());
    ASSERT_FALSE(add_op.ok());
    ASSERT_EQ("Document's `id` field should be a string.", add_op.error());

    collectionManager.drop_collection("coll1");
}

TEST_F(CollectionSpecificTest, CreateManyCollectionsAndDeleteOneOfThem) {
    std::vector<field> fields = {field("title", field_types::STRING, false),
                                 field("points", field_types::INT32, false),};

    for(size_t i = 0; i <= 10; i++) {
        const std::string& coll_name = "coll" + std::to_string(i);
        collectionManager.drop_collection(coll_name);
        ASSERT_TRUE(collectionManager.create_collection(coll_name, 1, fields, "points").ok());
    }

    auto coll1 = collectionManager.get_collection_unsafe("coll1");
    auto coll10 = collectionManager.get_collection_unsafe("coll10");

    nlohmann::json doc;
    doc["id"] = "0";
    doc["title"] = "The quick brown fox was too fast.";
    doc["points"] = 100;

    ASSERT_TRUE(coll1->add(doc.dump()).ok());
    ASSERT_TRUE(coll10->add(doc.dump()).ok());

    collectionManager.drop_collection("coll1", true);

    // Record with id "0" should exist in coll10
    ASSERT_TRUE(coll10->get("0").ok());

    for(size_t i = 0; i <= 10; i++) {
        const std::string& coll_name = "coll" + std::to_string(i);
        collectionManager.drop_collection(coll_name);
    }
}

TEST_F(CollectionSpecificTest, DeleteOverridesAndSynonymsOnDiskDuringCollDrop) {
    std::vector<field> fields = {field("title", field_types::STRING, false),
                                 field("points", field_types::INT32, false),};

    for (size_t i = 0; i <= 10; i++) {
        const std::string& coll_name = "coll" + std::to_string(i);
        collectionManager.drop_collection(coll_name);
        ASSERT_TRUE(collectionManager.create_collection(coll_name, 1, fields, "points").ok());
    }

    auto coll1 = collectionManager.get_collection_unsafe("coll1");

    nlohmann::json override_json = {
        {"id",   "exclude-rule"},
        {
            "rule", {
                 {"query", "of"},
                 {"match", override_t::MATCH_EXACT}
             }
        }
    };
    override_json["excludes"] = nlohmann::json::array();
    override_json["excludes"][0] = nlohmann::json::object();
    override_json["excludes"][0]["id"] = "4";

    override_json["excludes"][1] = nlohmann::json::object();
    override_json["excludes"][1]["id"] = "11";

    override_t override;
    override_t::parse(override_json, "", override);
    coll1->add_override(override);

    // add synonym
    synonym_t synonym1{"ipod-synonyms", {}, {{"ipod"}, {"i", "pod"}, {"pod"}} };
    coll1->add_synonym(synonym1);

    collectionManager.drop_collection("coll1");

    // overrides should have been deleted from the store
    std::vector<std::string> stored_values;
    store->scan_fill(Collection::COLLECTION_OVERRIDE_PREFIX, stored_values);
    ASSERT_TRUE(stored_values.empty());

    // synonyms should also have been deleted from the store
    store->scan_fill(Collection::COLLECTION_SYNONYM_PREFIX, stored_values);
    ASSERT_TRUE(stored_values.empty());
}

TEST_F(CollectionSpecificTest, SingleCharMatchFullFieldHighlight) {
    std::vector<field> fields = {field("title", field_types::STRING, false),
                                 field("points", field_types::INT32, false),};

    Collection* coll1 = collectionManager.create_collection("coll1", 1, fields, "points").get();

    nlohmann::json doc1;
    doc1["id"] = "0";
    doc1["title"] = "Which of the following is a probable sign of infection?";
    doc1["points"] = 100;

    ASSERT_TRUE(coll1->add(doc1.dump()).ok());

    auto results = coll1->search("a 3-month", {"title"}, "", {}, {}, {2}, 10,
                                 1, FREQUENCY, {false}, 1,
                                 spp::sparse_hash_set<std::string>(),
                                 spp::sparse_hash_set<std::string>(), 10, "", 30, 5,
                                 "title", 1).get();

    ASSERT_EQ(1, results["hits"].size());
    ASSERT_EQ("0", results["hits"][0]["document"]["id"].get<std::string>());

    ASSERT_EQ("Which of the following is <mark>a</mark> probable sign of infection?",
              results["hits"][0]["highlights"][0]["snippet"].get<std::string>());

    ASSERT_EQ("Which of the following is <mark>a</mark> probable sign of infection?",
                 results["hits"][0]["highlights"][0]["value"].get<std::string>());

    collectionManager.drop_collection("coll1");
}

TEST_F(CollectionSpecificTest, TokensSpreadAcrossFields) {
    std::vector<field> fields = {field("title", field_types::STRING, false),
                                 field("description", field_types::STRING, false),
                                 field("points", field_types::INT32, false),};

    Collection* coll1 = collectionManager.create_collection("coll1", 1, fields, "points").get();

    nlohmann::json doc1;
    doc1["id"] = "0";
    doc1["title"] = "Foo bar baz";
    doc1["description"] = "Share information with this device.";
    doc1["points"] = 100;

    nlohmann::json doc2;
    doc2["id"] = "1";
    doc2["title"] = "Foo Random";
    doc2["description"] = "The Bar Fox";
    doc2["points"] = 250;

    ASSERT_TRUE(coll1->add(doc1.dump()).ok());
    ASSERT_TRUE(coll1->add(doc2.dump()).ok());

    auto results = coll1->search("foo bar", {"title", "description"}, "", {}, {}, {0}, 10,
                                 1, FREQUENCY, {false, false},
                                 10, spp::sparse_hash_set<std::string>(),
                                 spp::sparse_hash_set<std::string>(), 10, "", 30, 4, "", 40, {}, {}, {}, 0,
                                 "<mark>", "</mark>", {4, 1}).get();

    ASSERT_EQ("0", results["hits"][0]["document"]["id"].get<std::string>());
    ASSERT_EQ("1", results["hits"][1]["document"]["id"].get<std::string>());

    collectionManager.drop_collection("coll1");
}

TEST_F(CollectionSpecificTest, GuardAgainstIdFieldInSchema) {
    // The "id" field, if defined in the schema should be ignored

    std::vector<field> fields = {field("title", field_types::STRING, false),
                                 field("id", field_types::STRING, false),
                                 field("points", field_types::INT32, false),};

    nlohmann::json schema;
    schema["name"] = "books";
    schema["fields"] = nlohmann::json::array();
    schema["fields"][0]["name"] = "title";
    schema["fields"][0]["type"] = "string";
    schema["fields"][1]["name"] = "id";
    schema["fields"][1]["type"] = "string";
    schema["fields"][2]["name"] = "points";
    schema["fields"][2]["type"] = "int32";

    Collection* coll1 = collectionManager.create_collection(schema).get();

    ASSERT_EQ(0, coll1->get_schema().count("id"));

    collectionManager.drop_collection("coll1");
}

TEST_F(CollectionSpecificTest, HandleBadCharactersInStringGracefully) {
    std::vector<field> fields = {field("title", field_types::STRING, false),
                                 field("points", field_types::INT32, false),};

    Collection* coll1 = collectionManager.create_collection("coll1", 1, fields, "points").get();
    std::string doc_str = "不推荐。\",\"price\":10.12,\"ratings\":5}";

    auto add_op = coll1->add(doc_str);
    ASSERT_FALSE(add_op.ok());

    collectionManager.drop_collection("coll1");
}

TEST_F(CollectionSpecificTest, HighlightSecondaryFieldWithPrefixMatch) {
    std::vector<field> fields = {field("title", field_types::STRING, false),
                                 field("description", field_types::STRING, false),
                                 field("points", field_types::INT32, false),};

    Collection* coll1 = collectionManager.create_collection("coll1", 1, fields, "points").get();

    nlohmann::json doc1;
    doc1["id"] = "0";
    doc1["title"] = "Functions and Equations";
    doc1["description"] = "Use a function to solve an equation.";
    doc1["points"] = 100;

    nlohmann::json doc2;
    doc2["id"] = "1";
    doc2["title"] = "Function of effort";
    doc2["description"] = "Learn all about it.";
    doc2["points"] = 100;

    ASSERT_TRUE(coll1->add(doc1.dump()).ok());
    ASSERT_TRUE(coll1->add(doc2.dump()).ok());

    auto results = coll1->search("function", {"title", "description"}, "", {}, {}, {0}, 10,
                                 1, FREQUENCY, {true, true},
                                 10, spp::sparse_hash_set<std::string>(),
                                 spp::sparse_hash_set<std::string>(), 10, "", 30, 4, "", 40, {}, {}, {}, 0,
                                 "<mark>", "</mark>", {1, 1}).get();

    ASSERT_EQ("0", results["hits"][0]["document"]["id"].get<std::string>());

    ASSERT_EQ(2, results["hits"][0]["highlights"].size());

    ASSERT_EQ("<mark>Functions</mark> and Equations",
              results["hits"][0]["highlights"][0]["snippet"].get<std::string>());

    ASSERT_EQ("Use a <mark>function</mark> to solve an equation.",
              results["hits"][0]["highlights"][1]["snippet"].get<std::string>());

    collectionManager.drop_collection("coll1");
}

TEST_F(CollectionSpecificTest, HighlightWithDropTokens) {
    std::vector<field> fields = {field("description", field_types::STRING, false),
                                 field("points", field_types::INT32, false),};

    Collection* coll1 = collectionManager.create_collection("coll1", 1, fields, "points").get();

    nlohmann::json doc1;
    doc1["id"] = "0";
    doc1["description"] = "HPE Aruba AP-575 802.11ax Wireless Access Point - TAA Compliant - 2.40 GHz, "
                          "5 GHz - MIMO Technology - 1 x Network (RJ-45) - Gigabit Ethernet - Bluetooth 5";
    doc1["points"] = 100;

    ASSERT_TRUE(coll1->add(doc1.dump()).ok());

    auto results = coll1->search("HPE Aruba AP-575 Technology Gigabit Bluetooth 5", {"description"}, "", {}, {}, {0}, 10,
                                 1, FREQUENCY, {true},
                                 10, spp::sparse_hash_set<std::string>(),
                                 spp::sparse_hash_set<std::string>(), 10, "", 30, 4, "description", 40, {}, {}, {}, 0,
                                 "<mark>", "</mark>").get();

    ASSERT_EQ(1, results["hits"][0]["highlights"].size());
    ASSERT_EQ("0", results["hits"][0]["document"]["id"].get<std::string>());

    ASSERT_EQ("<mark>HPE</mark> <mark>Aruba</mark> <mark>AP-575</mark> 802.11ax Wireless Access Point - "
              "TAA Compliant - 2.40 GHz, <mark>5</mark> GHz - MIMO <mark>Technology</mark> - 1 x Network (RJ-45) - "
              "<mark>Gigabit</mark> Ethernet - <mark>Bluetooth</mark> <mark>5</mark>",
              results["hits"][0]["highlights"][0]["snippet"].get<std::string>());

    collectionManager.drop_collection("coll1");
}

TEST_F(CollectionSpecificTest, HighlightLongFieldWithDropTokens) {
    std::vector<field> fields = {field("description", field_types::STRING, false),
                                 field("points", field_types::INT32, false),};

    Collection* coll1 = collectionManager.create_collection("coll1", 1, fields, "points").get();

    nlohmann::json doc1;
    doc1["id"] = "0";
    doc1["description"] = "Tripp Lite USB C to VGA Multiport Video Adapter Converter w/ USB-A Hub, USB-C PD Charging "
                          "Port & Gigabit Ethernet Port, Thunderbolt 3 Compatible, USB Type C to VGA, USB-C, USB "
                          "Type-C - for Notebook/Tablet PC - 2 x USB Ports - 2 x USB 3.0 - "
                          "Network (RJ-45) - VGA - Wired";
    doc1["points"] = 100;

    ASSERT_TRUE(coll1->add(doc1.dump()).ok());

    auto results = coll1->search("wired charging gigabit port", {"description"}, "", {}, {}, {0}, 10,
                                 1, FREQUENCY, {true},
                                 1, spp::sparse_hash_set<std::string>(),
                                 spp::sparse_hash_set<std::string>(), 10, "", 30, 4, "description", 1, {}, {}, {}, 0,
                                 "<mark>", "</mark>").get();

    ASSERT_EQ(1, results["hits"][0]["highlights"].size());
    ASSERT_EQ("0", results["hits"][0]["document"]["id"].get<std::string>());

    ASSERT_EQ("Tripp Lite USB C to VGA Multiport Video Adapter Converter w/ USB-A Hub, "
              "USB-C PD <mark>Charging</mark> <mark>Port</mark> & <mark>Gigabit</mark> Ethernet "
              "<mark>Port,</mark> Thunderbolt 3 Compatible, USB Type C to VGA, USB-C, USB Type-C - for "
              "Notebook/Tablet PC - 2 x USB <mark>Ports</mark> - 2 x USB 3.0 - Network (RJ-45) - "
              "VGA - <mark>Wired</mark>",
              results["hits"][0]["highlights"][0]["value"].get<std::string>());

    collectionManager.drop_collection("coll1");
}

TEST_F(CollectionSpecificTest, HighlightWithDropTokensAndPrefixSearch) {
    std::vector<field> fields = {field("username", field_types::STRING, false),
                                 field("name", field_types::STRING, false),
                                 field("points", field_types::INT32, false),};

    Collection* coll1 = collectionManager.create_collection("coll1", 1, fields, "points").get();

    nlohmann::json doc1;
    doc1["id"] = "0";
    doc1["username"] = "Pandaabear";
    doc1["name"] = "Panda's Basement";
    doc1["points"] = 100;

    nlohmann::json doc2;
    doc2["id"] = "1";
    doc2["username"] = "Pandaabear";
    doc2["name"] = "Pandaabear Basic";
    doc2["points"] = 100;

    ASSERT_TRUE(coll1->add(doc1.dump()).ok());
    ASSERT_TRUE(coll1->add(doc2.dump()).ok());

    auto results = coll1->search("pandaabear bas", {"username", "name"},
                                 "", {}, {}, {2, 2}, 10,
                                 1, FREQUENCY, {true, true},
                                 1, spp::sparse_hash_set<std::string>(),
                                 spp::sparse_hash_set<std::string>(), 10, "", 30, 4, "", 1, {}, {}, {}, 0,
                                 "<mark>", "</mark>").get();

    ASSERT_EQ(2, results["hits"][0]["highlights"].size());
    ASSERT_EQ("1", results["hits"][0]["document"]["id"].get<std::string>());
    ASSERT_EQ("0", results["hits"][1]["document"]["id"].get<std::string>());

    ASSERT_EQ(2, results["hits"][1]["highlights"].size());

    ASSERT_EQ("<mark>Pandaabear</mark>",
              results["hits"][1]["highlights"][0]["snippet"].get<std::string>());

    ASSERT_EQ("Panda's <mark>Basement</mark>",
              results["hits"][1]["highlights"][1]["snippet"].get<std::string>());

    collectionManager.drop_collection("coll1");
}

TEST_F(CollectionSpecificTest, PrefixSearchOnlyOnLastToken) {
    std::vector<field> fields = {field("concat", field_types::STRING, false),
                                 field("points", field_types::INT32, false),};

    Collection* coll1 = collectionManager.create_collection("coll1", 1, fields, "points").get();

    nlohmann::json doc1;
    doc1["id"] = "0";
    doc1["concat"] = "SPZ005 SPACEPOLE Spz005 Space Pole Updated!!! Accessories Stands & Equipment Cabinets POS "
                     "Terminal Stand Spacepole 0 SPZ005";
    doc1["points"] = 100;

    ASSERT_TRUE(coll1->add(doc1.dump()).ok());

    auto results = coll1->search("spz space", {"concat"},
                                 "", {}, {}, {1}, 10,
                                 1, FREQUENCY, {true},
                                 0, spp::sparse_hash_set<std::string>(),
                                 spp::sparse_hash_set<std::string>(), 10, "", 30, 4, "concat", 20, {}, {}, {}, 0,
                                 "<mark>", "</mark>").get();

    ASSERT_EQ(0, results["hits"][0]["highlights"].size());

    collectionManager.drop_collection("coll1");
}

TEST_F(CollectionSpecificTest, TokenStartingWithSameLetterAsPrevToken) {
    std::vector<field> fields = {field("name", field_types::STRING, false),
                                 field("points", field_types::INT32, false),};

    Collection* coll1 = collectionManager.create_collection("coll1", 1, fields, "points").get();

    nlohmann::json doc1;
    doc1["id"] = "0";
    doc1["name"] = "John Jack";
    doc1["points"] = 100;

    nlohmann::json doc2;
    doc2["id"] = "1";
    doc2["name"] = "John Williams";
    doc2["points"] = 100;

    ASSERT_TRUE(coll1->add(doc1.dump()).ok());
    ASSERT_TRUE(coll1->add(doc2.dump()).ok());

    auto results = coll1->search("john j", {"name"},
                                 "", {}, {}, {2}, 10,
                                 1, FREQUENCY, {true},
                                 10, spp::sparse_hash_set<std::string>(),
                                 spp::sparse_hash_set<std::string>(), 10, "", 30, 4, "", 10, {}, {}, {}, 0,
                                 "<mark>", "</mark>").get();

    ASSERT_EQ(2, results["hits"].size());
    ASSERT_EQ("0", results["hits"][0]["document"]["id"].get<std::string>());
    ASSERT_EQ("1", results["hits"][1]["document"]["id"].get<std::string>());

    collectionManager.drop_collection("coll1");
}