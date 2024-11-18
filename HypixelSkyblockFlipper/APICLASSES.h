#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <chrono>
#include <regex>
#include <map>
#include <cstdlib>  // For std::stoi

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>

#ifdef _DEBUG
#undef _DEBUG
#include <Python.h>
#define _DEBUG
#else
#include <Python.h>
#endif

class ThreadPool {
public:
    // Constructor to initialize the thread pool with a given number of threads
    ThreadPool(size_t numThreads) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this]() {
                while (true) {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(queueMutex);
                        condition.wait(lock, [this]() { return !tasks.empty() || stop; });

                        if (stop && tasks.empty()) return;

                        task = std::move(tasks.front());
                        tasks.pop();
                    }

                    // Execute the task
                    task();
                }
                });
        }
    }

    // Destructor to join all threads when the ThreadPool object is destroyed
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stop = true;
        }

        condition.notify_all();
        for (std::thread& worker : workers) {
            worker.join();
        }
    }

    // Add a new task to the queue
    template <typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
        using returnType = typename std::result_of<F(Args...)>::type;

        auto task = std::make_shared<std::packaged_task<returnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<returnType> res = task->get_future();

        {
            std::unique_lock<std::mutex> lock(queueMutex);
            tasks.emplace([task]() { (*task)(); });
        }

        condition.notify_one();
        return res;
    }

private:
    std::vector<std::thread> workers;                  // Worker threads
    std::queue<std::function<void()>> tasks;           // Task queue
    std::mutex queueMutex;                             // Mutex to synchronize access to the task queue
    std::condition_variable condition;                 // Condition variable to notify threads
    bool stop = false;                                 // Flag to stop the threads
};

class ItemPrice {
public:
    // Default constructor
    ItemPrice() = default;

    // Constructor from JSON
    explicit ItemPrice(const nlohmann::json& json) {
        from_json(json, *this);
    }

    // Convert to JSON
    nlohmann::json to_json() const {
        nlohmann::json json;
        json["min"] = min;
        json["max"] = max;
        json["avg"] = avg;
        json["volume"] = volume;
        json["time"] = time;
        return json;
    }

    // JSON deserialization
    friend void from_json(const nlohmann::json& json, ItemPrice& itemPrice) {
        itemPrice.min = json.at("min").get<float>();
        itemPrice.max = json.at("max").get<float>();
        itemPrice.avg = json.at("avg").get<float>();
        itemPrice.volume = json.at("volume").get<float>();
        itemPrice.time = json.at("time").get<std::string>();
    }

    // Static method to parse a JSON response into a vector of ItemPrice
    static bool ParseFromJson(const std::string& jsonResponse, std::vector<ItemPrice>& itemPrices) {
        try {
            // Parse the JSON response
            nlohmann::json jsonResponseParsed = nlohmann::json::parse(jsonResponse);

            // Ensure the JSON response is an array
            if (jsonResponseParsed.is_array()) {
                // Clear the itemPrices vector
                itemPrices.clear();

                // Iterate through the JSON array and populate ItemPrice objects
                for (const auto& jsonItem : jsonResponseParsed) {
                    ItemPrice itemPrice = jsonItem.get<ItemPrice>();
                    itemPrices.push_back(itemPrice);
                }
                // For debugging, print the number of items processed
                //std::cout << "Number of ItemPrices: " << itemPrices.size() << std::endl;
            }
            else {
                //std::cerr << "Unexpected JSON format: not an array." << std::endl;
                return false;
            }
        }
        catch (const std::exception& e) {
          //  std::cerr << "Error parsing JSON: " << e.what() << std::endl;
            return false;
        }

        return true;
    }
    
    static double CalculateFilteredAverageStartingBid(std::vector<ItemPrice>& items) {
        if (items.empty()) return 0;
        int volume = 0;
        for (auto item : items)
        {
            volume += item.volume;
        }
        
        return volume / items.size();
    }

    // Member variables
    float min;
    float max;
    float avg;
    float volume;
    std::string time;
};

class AuctionData {
public:
    bool success;
    int page;
    int totalPages;
    int totalAuctions;
    long long lastUpdated;

    // Default constructor
    AuctionData()
        : success(false), page(0), totalPages(0), totalAuctions(0), lastUpdated(0) {}

    // Constructor
    AuctionData(bool success, int page, int totalPages, int totalAuctions, long long lastUpdated)
        : success(success), page(page), totalPages(totalPages), totalAuctions(totalAuctions), lastUpdated(lastUpdated) {}

    // Define how to deserialize the JSON into the AuctionData object
   static void from_json(const nlohmann::json& j, AuctionData& data) {
        data.success = j.at("success").get<bool>();           // Extract "success" field as bool
        data.page = j.at("page").get<int>();                 // Extract "page" field as int
        data.totalPages = j.at("totalPages").get<int>();     // Extract "totalPages" field as int
        data.totalAuctions = j.at("totalAuctions").get<int>();  // Extract "totalAuctions" field as int
        data.lastUpdated = j.at("lastUpdated").get<long long>();  // Extract "lastUpdated" field as long long
    }

   static bool ParseFromJson(const std::string& jsonResponse, AuctionData& auctiondata) {
       try {
           // Parse the JSON response
           nlohmann::json jsonResponseParsed = nlohmann::json::parse(jsonResponse);

           AuctionData item;
           try {
               AuctionData::from_json(jsonResponseParsed, item);
               auctiondata = item;
               // std::cout << "Parsed item at index " << i << std::endl;
           }
           catch (const std::exception& e) {
               //std::cerr << "Error parsing item at index " << i << ": " << e.what() << std::endl;
           }
       }
        catch (const std::exception& e) {
            return false;
        }

        return true;
    }
};

class AuctionItem {
public:

    // Nested FlatNbt struct inside AuctionItem
    struct FlatNbt {
        std::string rarity_upgrades;
        // std::string JASPER_0;
        // std::string COMBAT_0;
        std::string unlocked_slots;
        //   std::string COMBAT_0_gem;
        std::string upgrade_level;
        std::string uid;
        std::string uuid;
        //std::string dye_item;
        //std::string color;
        //std::string cc;

    };

    // Default constructor
    AuctionItem() = default;

    // Constructor from JSON
    explicit AuctionItem(const nlohmann::json& json) {
        from_json(json, *this);
    }

    static void from_jsonCustom(const nlohmann::json& json, AuctionItem& item)
    {

        // Deserialize the "flatNbt" object directly inside AuctionItem
        if (json.contains("flatNbt") && !json["flatNbt"].is_null()) {
            auto flatNbt = json["flatNbt"];
            if (flatNbt.contains("rarity_upgrades")) {
                item.flatNbt.rarity_upgrades = flatNbt.at("rarity_upgrades").get<std::string>();
            }

            if (flatNbt.contains("unlocked_slots")) {
                item.flatNbt.unlocked_slots = flatNbt.at("unlocked_slots").get<std::string>();
            }

            if (flatNbt.contains("upgrade_level")) {
                item.flatNbt.upgrade_level = flatNbt.at("upgrade_level").get<std::string>();
            }
            else
            {
                item.flatNbt.upgrade_level = "-1";
            }
            if (flatNbt.contains("uid")) {
                item.flatNbt.uid = flatNbt.at("uid").get<std::string>();
            }
            if (flatNbt.contains("uuid")) {
                item.flatNbt.uuid = flatNbt.at("uuid").get<std::string>();
            }
        }
   
    }

    // JSON deserialization
    static void from_json(const nlohmann::json& json, AuctionItem& item) {
        if (json.contains("uuid") && !json["uuid"].is_null())
            item.uuid = json.at("uuid").get<std::string>();
        if (json.contains("count") && !json["count"].is_null())
            item.count = json.at("count").get<int>();
        if (json.contains("startingBid") && !json["startingBid"].is_null())
            item.startingBid = json.at("startingBid").get<int>();
        if (json.contains("tag") && !json["tag"].is_null())
            item.tag = json.at("tag").get<std::string>();
        if (json.contains("itemName") && !json["itemName"].is_null())
            item.itemName = json.at("itemName").get<std::string>();
        if (json.contains("start") && !json["start"].is_null())
            item.start = json.at("start").get<std::string>();
        if (json.contains("end") && !json["end"].is_null())
            item.end = json.at("end").get<std::string>();
        if (json.contains("auctioneerId") && !json["auctioneerId"].is_null())
            item.auctioneerId = json.at("auctioneerId").get<std::string>();
        if (json.contains("profileId") && !json["profileId"].is_null())
            item.profileId = json.at("profileId").get<std::string>();

        if (json.contains("highestBidAmount") && !json["highestBidAmount"].is_null())
            item.highestBidAmount = json.at("highestBidAmount").get<int>();
        if (json.contains("anvilUses") && !json["anvilUses"].is_null())
            item.anvilUses = json.at("anvilUses").get<int>();
        if (json.contains("enchantments") && !json["enchantments"].is_null())
            item.enchantments = json.at("enchantments").get<std::vector<nlohmann::json>>();
        if (json.contains("itemCreatedAt") && !json["itemCreatedAt"].is_null())
            item.itemCreatedAt = json.at("itemCreatedAt").get<std::string>();
        if (json.contains("reforge") && !json["reforge"].is_null())
            item.reforge = json.at("reforge").get<std::string>();
        if (json.contains("category") && !json["category"].is_null())
            item.category = json.at("category").get<std::string>();
        if (json.contains("tier") && !json["tier"].is_null())
            item.tier = json.at("tier").get<std::string>();
        if (json.contains("bin") && !json["bin"].is_null())
            item.bin = json.at("bin").get<bool>();


        if (json.contains("nbtData") && !json["nbtData"].is_null())
        {
            auto nbtData = json["nbtData"];
            if (nbtData.contains("data"))
            {
                auto data = nbtData["data"];
                if (data.contains("attributes"))
                    item.attributes = true;
                else
                {
                    item.attributes = false;
                }
            }
        }

        // Deserialize the "flatNbt" object directly inside AuctionItem
        if (json.contains("flatNbt") && !json["flatNbt"].is_null()) {
            auto flatNbt = json["flatNbt"];
            if (flatNbt.contains("rarity_upgrades")) {
                item.flatNbt.rarity_upgrades = flatNbt.at("rarity_upgrades").get<std::string>();
            }

            if (flatNbt.contains("unlocked_slots")) {
                item.flatNbt.unlocked_slots = flatNbt.at("unlocked_slots").get<std::string>();
            }

            if (flatNbt.contains("upgrade_level")) {
                item.flatNbt.upgrade_level = flatNbt.at("upgrade_level").get<std::string>();
            }
            else
            {
                item.flatNbt.upgrade_level = "-1";
            }
            if (flatNbt.contains("uid")) {
                item.flatNbt.uid = flatNbt.at("uid").get<std::string>();
            }
            if (flatNbt.contains("uuid")) {
                item.flatNbt.uuid = flatNbt.at("uuid").get<std::string>();
            }
        }
    }

    static bool ParseFromJson(const std::string& jsonResponse, std::vector<AuctionItem>& auctionItems) {
        try {
            // Parse the JSON response
            nlohmann::json jsonResponseParsed = nlohmann::json::parse(jsonResponse);

            // Ensure the JSON response is an array
            if (jsonResponseParsed.is_array()) {
                // Clear the auctionItems vector
                auctionItems.clear();

                int size = jsonResponseParsed.size();
                // Iterate through the JSON array and populate AuctionItem objects
                for (size_t i = 0; i < jsonResponseParsed.size(); ++i) {
                    const auto& jsonItem = jsonResponseParsed[i];
                    AuctionItem item;
                    try {
                        AuctionItem::from_json(jsonItem, item);
                        auctionItems.push_back(item);
                       // std::cout << "Parsed item at index " << i << std::endl;
                    }
                    catch (const std::exception& e) {
                        //std::cerr << "Error parsing item at index " << i << ": " << e.what() << std::endl;
                    }
                }
            }
            else {
               // std::cerr << "Unexpected JSON format: not an array." << std::endl;
                return false;
            }
        }
        catch (const std::exception& e) {
           // std::cerr << "Error parsing JSON: " << e.what() << std::endl;
            return false;
        }

        return true;
    }

    static double CalculateFilteredAverageStartingBid(std::vector<AuctionItem>& items, double trimPercentage) {
        if (items.empty()) return 0.0;

        // Extract starting bids
        std::vector<double> bids;
        for (const auto& item : items) {
            bids.push_back(item.startingBid);
        }

        // Sort the bids
        std::sort(bids.begin(), bids.end());

        // Determine the number of elements to trim
        int trimCount = static_cast<int>(trimPercentage * bids.size());

        // Trim the outliers
        auto begin = bids.begin() + trimCount;
        auto end = bids.end() - trimCount;

        // Calculate the average of the trimmed range
        double sum = 0.0;
        for (auto it = begin; it != end; ++it) {
            sum += *it;
        }

        return sum / std::distance(begin, end);
    }


    


    // Member variables
    std::string uuid;
    int count;
    int startingBid;
    std::string tag;
    std::string itemName;
    std::string start;
    std::string end;
    std::string auctioneerId;
    std::string profileId;
    std::shared_ptr<std::string> coop;
    std::vector<std::string> coopMembers;
    std::vector<int> bids;
    int highestBidAmount;
    int anvilUses;
    std::vector<nlohmann::json> enchantments;
    nlohmann::json nbtData;
    std::string itemCreatedAt;
    std::string reforge;
    std::string category;
    std::string tier;
    bool bin;
    FlatNbt flatNbt;
    bool attributes;
};

class ItemData {
public:
    int id = -1;
    int count = -1;
    int upgrade_level = -1;
    std::string id_in_attributes = "Unknown";

    // Function to parse item data from a string
    void parseItemData(const std::string& input) {
        auto extractField = [](const std::string& input, const std::string& pattern, auto& field, auto defaultValue) {
            std::regex regex(pattern);
            std::smatch match;
            if (std::regex_search(input, match, regex)) {
                if constexpr (std::is_integral_v<std::remove_reference_t<decltype(field)>>) {
                    field = std::stoi(match[1].str());
                }
                else {
                    field = match[1].str();
                }
            }
            else {
                field = defaultValue;  // Use default value if not found
            }
            };

        // Extract fields using regex patterns
        extractField(input, R"('id':\s*Short\((\d+)\))", id, -1);  // Extract 'id' as integer
        extractField(input, R"('Count':\s*Byte\((\d+)\))", count, -1);  // Extract 'Count' as integer
        extractField(input, R"('upgrade_level':\s*Int\((\d+)\))", upgrade_level, -1);  // Extract 'upgrade_level'
        if (upgrade_level == -1) {  // If upgrade_level is not found, check for dungeon_item_level
            extractField(input, R"('dungeon_item_level':\s*Int\((\d+)\))", upgrade_level, -1);  // Extract 'dungeon_item_level'
        }
        extractField(input, R"('id':\s*String\('([^']+)'\))", id_in_attributes, "Unknown");

    }

};

class SkyblockItem {
public:
   
    // Default constructor
    SkyblockItem() = default;

    // Constructor from JSON
    explicit SkyblockItem(const  nlohmann::json& json) {
        from_json(json, *this);
    }

    // Convert to JSON
    nlohmann::json to_json() const {
        nlohmann::json j;
        j["material"] = material;
        j["durability"] = durability;
        j["skin"] = skin;
        j["name"] = name;
        j["category"] = category;
        j["tier"] = tier;
        j["npc_sell_price"] = npc_sell_price;
        j["id"] = id;
        return j;
    }

    // JSON deserialization
    friend void from_json(const  nlohmann::json& j, SkyblockItem& item) {
        item.material = j.at("material").get<std::string>();
        item.durability = j.at("durability").get<int>();
        item.skin = j.at("skin").get<std::string>();
        item.name = j.at("name").get<std::string>();
        item.category = j.at("category").get<std::string>();
        item.tier = j.at("tier").get<std::string>();
        item.npc_sell_price = j.at("npc_sell_price").get<int>();
        item.id = j.at("id").get<std::string>();
    }

    // Static method to parse a JSON response
    static bool ParseFromJson(const std::string& jsonResponse, std::vector<SkyblockItem>& items) {
        try {
            // Parse the JSON response
            nlohmann::json jsonResponseParsed = nlohmann::json::parse(jsonResponse);

            // Ensure the JSON response has the "items" field and is an array
            if (jsonResponseParsed.contains("items") && jsonResponseParsed["items"].is_array()) {
                // Clear the items vector
                items.clear();

                // Iterate through the JSON array and populate SkyblockItem objects
                for (const auto& jsonItem : jsonResponseParsed["items"]) {
                    SkyblockItem item = jsonItem.get<SkyblockItem>();
                    items.push_back(item);
                }
                // For debugging, print the number of items processed
                std::cout << "Number of Items: " << items.size() << std::endl;
            }
            else {
                std::cerr << "Unexpected JSON format: no 'items' field or not an array." << std::endl;
                return false;
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Error parsing JSON: " << e.what() << std::endl;
            return false;
        }

        return true;
    }

    // Member variables
    std::string material;
    int durability;
    std::string skin;
    std::string name;
    std::string category;
    std::string tier;
    int npc_sell_price;
    std::string id;

};

class Auction {
public:
    
    // Default constructor
    Auction() = default;

    // Constructor from JSON
    explicit Auction(const nlohmann::json& json) {
        from_json(json, *this);
    }

    // Convert to JSON
    nlohmann::json to_json() const {
        nlohmann::json json;
        json["uuid"] = uuid;
        json["auctioneer"] = auctioneer;
        json["profile_id"] = profile_id;
        json["coop"] = coop;
        json["start"] = start;
        json["end"] = end;
        json["item_name"] = item_name;
        json["item_lore"] = item_lore;
        json["extra"] = extra;
        json["category"] = category;
        json["tier"] = tier;
        json["starting_bid"] = starting_bid;
        json["item_bytes"] = item_bytes;
        json["claimed"] = claimed;
        json["claimed_bidders"] = claimed_bidders;
        json["highest_bid_amount"] = highest_bid_amount;
        json["bids"] = bids;
        json["bin"] = bin;
        return json;
    }

    // JSON deserialization
    friend void from_json(const nlohmann::json& json, Auction& auction) {
        auction.uuid = json.at("uuid").get<std::string>();
        auction.auctioneer = json.at("auctioneer").get<std::string>();
        auction.profile_id = json.at("profile_id").get<std::string>();
        auction.coop = json.at("coop").get<std::vector<std::string>>();
        auction.start = json.at("start").get<long long>();
        auction.end = json.at("end").get<long long>();
        auction.item_name = json.at("item_name").get<std::string>();
        auction.item_lore = json.at("item_lore").get<std::string>();
        auction.extra = json.at("extra").get<std::string>();
        auction.category = json.at("category").get<std::string>();
        auction.tier = json.at("tier").get<std::string>();
        auction.starting_bid = json.at("starting_bid").get<int>();
        auction.item_bytes = json.at("item_bytes");
        auction.claimed = json.at("claimed").get<bool>();
        auction.claimed_bidders = json.at("claimed_bidders").get<std::vector<std::string>>();
        auction.highest_bid_amount = json.at("highest_bid_amount").get<int>();
        auction.bids = json.at("bids").get<std::vector<nlohmann::json>>();
        auction.bin = json.at("bin").get<bool>();
    }

    void PythonGetData(std::string data, std::string& output)
    {
        // The base64-encoded data string
       // std::string da = "H4sIAAAAAAAA/41UyY7rxhWt7vde3K2F7WyzUgDbgCG0W5w0LN6CEsXmTFHiIHJjFMmSWFRxEEdJ/+BVPqI3+Yr+sCB8RoJss7iowrnn3Hvu5owAeAYPeAQAeHgEjzh++OMBfFkXbd48jMCnBp4+gWcJx0gk8FQPrH+NwGh/bgkx+xxVT+BRjsFPNLWE3JyavhwRpF8YhmJflugYDT82ihguZtno+AB+TG4lviKyLcqWwAbFw4bRtipKVDUY1c/gqUHXpq1Q/aedJ/C8x6ccfkMe/6lxUiyogtd2SrAQ9vC0hieVONq6jOzMoZKwU5TpNZjvJcrvzfqNvdBikglrXj2eXNETWBfNnat0rotOT7E63eM+bBVYtjOSpn13m9GpMFmIaJXM80q+rs6EXfqYuVlyG7I6z8MJx9mXudBRq53SwspKknZtNJOOURtluyPS2WrnbHHe1p6/799eOdLflaDp1leNRsd65WFtn4XcQi16K7C3FycIrW1bXCzWisUA8txBKU7Fwpn6UBcF7dq5UrxhzzvqxideeXXcg5cJh5pRmjPDVnZBS5tM13d5lB1PGtz6M89q3B6Z3blukWr7r0qg0bDzPGNj9dGhFbckvO7XaV1owrFf1Lp9DD3OdvuK2rTEbPFyHm1XWiDutnxMjvMSwtV1bkY7a1Lp+IBIZpyFQonKcv6aJAssTzebMmaqPIstLqwuU9pXk4t02Bt+erQXl8JYm1be0GYsstp+0uhieVFFbc/SoQXhfk12E6o/H6YzyKyK41bOJX9tHcq1m6jLQy/Ns9Kti4k6O5mqrsg0o5UaYRxBDxYkj2acJCavR0rdWxdfDSbSpuWMUznHK69bEtFUU9JhJB6u8v2MuNuF0MwiuIv8xbnYAe25pl6pcmJLkjujOPXoR7vFTPArUX9tcc5CuTlhH0UJXN9dTWfSRPPOjWYKvhZPKI9X1KxcbAxfzXq8mHGrt2um3wyZvZcHDmaK9fUJfHEhadFDjG7KFHoUiZhdEh54bKYbykhPN93We8PeTPXUr+V8dQvpoAzfXCVYyzN56Pt3vzcynRpeSk932LifWT89X317cwsEIzHSDW3aESPjHkeS0gUZqQOHS0LPwSZWzJDZfasuykgXHwwz8KjuTy5N8jATp/FBIU7mXmOP3ALPwma+u8WeM3jZESTtqChz73JazmViaHuHHfo1HjS1nBYYSrtpJBSdxvyPq2VUGWZuGmViFq+5NjhYXfzmsrHk3oL9gtaz4ZLUSHTBmZqCmAZvDmPc5aku8L3h6X3wtksDz7n6d542PYcybH7q2/7Np53+252mEHFB6lC+rXP6/UzrtkXLObU8Wl+/AvAEPhswQ+CXj/f/J4nACHwX47ok8PYMPmtFhZ6GvPkCfv14n/PjEJ7GPW6S8cc7pOcDNK5J0dTjPsFRMo5gPg4R+PuAD/oIxWOcj29FW433TVHBExrrKG/HTQH+NlDqAUNjGMe4wUUOyRg3KKt/G7aNPt65j3ey2crr/7r/6wBpsBpGrGB0LocajP6wuTYV5JumwmHboPrpW0aD7zV+97b5fcWv1e1QT+D78D+S36OCFBX4TtiIvKPZw+y2HQQ/zSkqDLll+HJkUPzCzij0sohm8CVazhaI46iQmTGfwXODM1Q3MCuH/P3Hb6OfMQCP4C8CzIbLwCcA/g0/UWAELwYAAA==";

        // Initialize Python interpreter
        Py_Initialize();

        // Import necessary Python modules
        PyRun_SimpleString("import sys");
        PyRun_SimpleString("import io");

        // Create a StringIO object to capture Python output
        PyObject* ioModule = PyImport_ImportModule("io");
        PyObject* stringIOClass = PyObject_GetAttrString(ioModule, "StringIO");
        PyObject* stringIO = PyObject_CallObject(stringIOClass, nullptr);

        // Redirect Python stdout to our StringIO object
        PyObject* sysModule = PyImport_ImportModule("sys");
        PyObject_SetAttrString(sysModule, "stdout", stringIO);

        // Add site-packages directory to sys.path
        PyRun_SimpleString("sys.path.append('C:/Users/1/AppData/Local/Programs/Python/Python313/Lib/site-packages')");

        // Construct the Python code to execute
        std::string python_code =
            "import base64, zlib, nbtlib\n"
            "data = '''" + data + "'''\n"
            "decoded = base64.b64decode(data)\n"
            "decompressed = zlib.decompress(decoded, zlib.MAX_WBITS | 16)\n"
            "with open('output.nbt', 'wb') as f:\n"
            "    f.write(decompressed)\n"
            "nbt_data = nbtlib.load('output.nbt')\n"
            "print(nbt_data)";

        // Execute the Python code
        PyRun_SimpleString(python_code.c_str());

        // Retrieve the captured output from StringIO
        PyObject* result = PyObject_CallMethod(stringIO, "getvalue", nullptr);
        const char* captured_output = PyUnicode_AsUTF8(result);

        if (captured_output != nullptr) {
            output = captured_output; // Transfer to std::string
        }


        return;
    }

    // Static method to parse a JSON response
    static bool ParseFromJson(const std::string& jsonResponse, std::vector<Auction>& auctions) {
        try {
            // Parse the JSON response
            nlohmann::json jsonResponseParsed = nlohmann::json::parse(jsonResponse);

            // Ensure the JSON response has the "auctions" field and is an array
            if (jsonResponseParsed.contains("auctions") && jsonResponseParsed["auctions"].is_array()) {
                // Clear the auctions vector
                auctions.clear();

                // Iterate through the JSON array and populate Auction objects
                for (const auto& jsonItem : jsonResponseParsed["auctions"]) {
                    if (auctions.size() > 80) break;
                    Auction auction = jsonItem.get<Auction>();
                    if (!auction.bin) continue; 
                    std::string data;
                    auction.PythonGetData(auction.item_bytes, data);
                    auction.auctionitem.parseItemData(data);
                    auctions.push_back(auction);
                }

                // For debugging, print the number of items processed
                //std::cout << "Number of Auctions: " << auctions.size() << std::endl;
            }
            else {
               // std::cerr << "Unexpected JSON format: no 'auctions' field or not an array." << std::endl;
                return false;
            }
        }
        catch (const std::exception& e) {
           // std::cerr << "Error parsing JSON: " << e.what() << std::endl;
            return false;
        }

        return true;
    }



    // Member variables
    std::string uuid;
    std::string auctioneer;
    std::string profile_id;
    std::vector<std::string> coop;
    long long start;
    long long end;
    std::string item_name;
    std::string item_lore;
    std::string extra;
    std::string category;
    std::string tier;
    int starting_bid;
    std::string item_bytes;
    bool claimed;
    std::vector<std::string> claimed_bidders;
    int highest_bid_amount;
    std::vector<nlohmann::json> bids;
    bool bin;
    ItemData auctionitem;
};

class Reforges {
public:
    // Enum for Reforge options
    enum class Reforge {
        None,
        Ancient,
        Fierce,
        Wise,
        Necrotic,
        Heroic,
        Pure,
        Fabled,
        Titanic,
        Mythic,
        Spicy,
        Sharp,
        Giant,
        Withered,
        Jaded,
        Light,
        Blessed,
        Smart,
        Heavy,
        Clean,
        Bustling,
        Legendary,
        Loving,
        Spiritual,
        Auspicious,
        Rapid,
        Epic,
        Strengthened,
        Fast,
        Rooted,
        Fair,
        Glistening,
        OddSword,
        Unreal,
        Gentle,
        Waxed,
        Blooming,
        Renowned,
        Hasty,
        Precise,
        Strange,
        Deadly,
        Excellent,
        Reinforced,
        Submerged,
        Pitchin,
        Heated,
        Grand,
        Suspicious,
        Fine,
        Neat,
        RichBow,
        Salty,
        Awkward,
        Mossy,
        Robust,
        Dirty,
        Shaded,
        DoubleBit,
        Strong,
        Lumberjack,
        Godly,
        Spiked,
        Fleet,
        Refined,
        Bountiful,
        Forceful,
        Menacing,
        Perfect,
        Unpleasant,
        Brilliant,
        Fortunate,
        Lucky,
        Lush,
        Candied,
        Toil,
        Fruitful,
        Superior,
        Hurtful,
        Prospector,
        Festive,
        Treacherous,
        Itchy,
        Unyielding,
        Sturdy,
        Blended,
        Moil,
        AoteStone,
        GreenThumb,
        Stiff,
        Warped,
        Buzzing,
        WarpedOnAote,
        Snowy,
        Peasant,
        Gilded,
        Honored,
        Magnetic,
        Earthy,
        Bloody,
        Zealous,
        Great,
        Bizarre,
        Fortified,
        Headstrong,
        Unknown,
        Mithraic,
        Demonic,
        Zooming,
        Silky,
        Colossal,
        Rugged,
        Shiny,
        Ridiculous,
        Chomp,
        Ambered,
        Soft,
        Cubic,
        Empowered,
        Stellar,
        Vivid,
        Stained,
        Keen,
        Astute,
        Beady,
        BloodSoaked,
        GreaterSpook,
        Hefty,
        Pleasant,
        Simple,
        JerryStone,
        Bulky,
        Ominous,
        Pretty,
        RichSword,
        Fanged,
        Coldfusion,
        Sweet,
        OddBow,
        Any
    };


    // Helper function to convert enum to string
    static std::string reforge_to_string(Reforge reforge) {
        static const std::unordered_map<Reforge, std::string> reforge_to_string_map = {
            {Reforge::None, "None"},
            {Reforge::Ancient, "ancient"},
            {Reforge::Fierce, "Fierce"},
            {Reforge::Wise, "Wise"},
            {Reforge::Necrotic, "Necrotic"},
            {Reforge::Heroic, "Heroic"},
            {Reforge::Pure, "Pure"},
            {Reforge::Fabled, "Fabled"},
            {Reforge::Titanic, "Titanic"},
            {Reforge::Mythic, "Mythic"},
            {Reforge::Spicy, "Spicy"},
            {Reforge::Sharp, "Sharp"},
            {Reforge::Giant, "Giant"},
            {Reforge::Withered, "Withered"},
            {Reforge::Jaded, "Jaded"},
            {Reforge::Light, "Light"},
            {Reforge::Blessed, "Blessed"},
            {Reforge::Smart, "Smart"},
            {Reforge::Heavy, "Heavy"},
            {Reforge::Clean, "Clean"},
            {Reforge::Bustling, "bustling"},
            {Reforge::Legendary, "Legendary"},
            {Reforge::Loving, "Loving"},
            {Reforge::Spiritual, "Spiritual"},
            {Reforge::Auspicious, "Auspicious"},
            {Reforge::Rapid, "Rapid"},
            {Reforge::Epic, "Epic"},
            {Reforge::Strengthened, "strengthened"},
            {Reforge::Fast, "Fast"},
            {Reforge::Rooted, "rooted"},
            {Reforge::Fair, "Fair"},
            {Reforge::Glistening, "glistening"},
            {Reforge::OddSword, "odd_sword"},
            {Reforge::Unreal, "Unreal"},
            {Reforge::Gentle, "Gentle"},
            {Reforge::Waxed, "waxed"},
            {Reforge::Blooming, "blooming"},
            {Reforge::Renowned, "Renowned"},
            {Reforge::Hasty, "Hasty"},
            {Reforge::Precise, "Precise"},
            {Reforge::Strange, "Strange"},
            {Reforge::Deadly, "Deadly"},
            {Reforge::Excellent, "excellent"},
            {Reforge::Reinforced, "Reinforced"},
            {Reforge::Submerged, "submerged"},
            {Reforge::Pitchin, "pitchin"},
            {Reforge::Heated, "heated"},
            {Reforge::Grand, "Grand"},
            {Reforge::Suspicious, "suspicious"},
            {Reforge::Fine, "Fine"},
            {Reforge::Neat, "Neat"},
            {Reforge::RichBow, "rich_bow"},
            {Reforge::Salty, "Salty"},
            {Reforge::Awkward, "awkward"},
            {Reforge::Mossy, "mossy"},
            {Reforge::Robust, "robust"},
            {Reforge::Dirty, "dirty"},
            {Reforge::Shaded, "shaded"},
            {Reforge::DoubleBit, "double_bit"},
            {Reforge::Strong, "Strong"},
            {Reforge::Lumberjack, "lumberjack"},
            {Reforge::Godly, "Godly"},
            {Reforge::Spiked, "Spiked"},
            {Reforge::Fleet, "fleet"},
            {Reforge::Refined, "Refined"},
            {Reforge::Bountiful, "bountiful"},
            {Reforge::Forceful, "Forceful"},
            {Reforge::Menacing, "menacing"},
            {Reforge::Perfect, "Perfect"},
            {Reforge::Unpleasant, "Unpleasant"},
            {Reforge::Brilliant, "brilliant"},
            {Reforge::Fortunate, "fortunate"},
            {Reforge::Lucky, "lucky"},
            {Reforge::Lush, "lush"},
            {Reforge::Candied, "candied"},
            {Reforge::Toil, "toil"},
            {Reforge::Fruitful, "fruitful"},
            {Reforge::Superior, "Superior"},
            {Reforge::Hurtful, "Hurtful"},
            {Reforge::Prospector, "prospector"},
            {Reforge::Festive, "festive"},
            {Reforge::Treacherous, "Treacherous"},
            {Reforge::Itchy, "Itchy"},
            {Reforge::Unyielding, "unyielding"},
            {Reforge::Sturdy, "sturdy"},
            {Reforge::Blended, "blended"},
            {Reforge::Moil, "moil"},
            {Reforge::AoteStone, "aote_stone"},
            {Reforge::GreenThumb, "green_thumb"},
            {Reforge::Stiff, "stiff"},
            {Reforge::Warped, "warped"},
            {Reforge::Buzzing, "buzzing"},
            {Reforge::WarpedOnAote, "warped_on_aote"},
            {Reforge::Snowy, "snowy"},
            {Reforge::Peasant, "peasant"},
            {Reforge::Gilded, "Gilded"},
            {Reforge::Honored, "honored"},
            {Reforge::Magnetic, "Magnetic"},
            {Reforge::Earthy, "earthy"},
            {Reforge::Bloody, "bloody"},
            {Reforge::Zealous, "Zealous"},
            {Reforge::Great, "great"},
            {Reforge::Bizarre, "Bizarre"},
            {Reforge::Fortified, "fortified"},
            {Reforge::Headstrong, "headstrong"},
            {Reforge::Unknown, "Unknown"},
            {Reforge::Mithraic, "mithraic"},
            {Reforge::Demonic, "Demonic"},
            {Reforge::Zooming, "zooming"},
            {Reforge::Silky, "Silky"},
            {Reforge::Colossal, "colossal"},
            {Reforge::Rugged, "rugged"},
            {Reforge::Shiny, "Shiny"},
            {Reforge::Ridiculous, "ridiculous"},
            {Reforge::Chomp, "chomp"},
            {Reforge::Ambered, "ambered"},
            {Reforge::Soft, "soft"},
            {Reforge::Cubic, "cubic"},
            {Reforge::Empowered, "empowered"},
            {Reforge::Stellar, "stellar"},
            {Reforge::Vivid, "Vivid"},
            {Reforge::Stained, "stained"},
            {Reforge::Keen, "Keen"},
            {Reforge::Astute, "astute"},
            {Reforge::Beady, "beady"},
            {Reforge::BloodSoaked, "blood_soaked"},
            {Reforge::GreaterSpook, "greater_spook"},
            {Reforge::Hefty, "hefty"},
            {Reforge::Pleasant, "Pleasant"},
            {Reforge::Simple, "Simple"},
            {Reforge::JerryStone, "jerry_stone"},
            {Reforge::Bulky, "bulky"},
            {Reforge::Ominous, "Ominous"},
            {Reforge::Pretty, "Pretty"},
            {Reforge::RichSword, "rich_sword"},
            {Reforge::Fanged, "fanged"},
            {Reforge::Coldfusion, "coldfusion"},
            {Reforge::Sweet, "sweet"},
            {Reforge::OddBow, "odd_bow"},
            {Reforge::Any, "Any"}
        };
        auto it = reforge_to_string_map.find(reforge);
        if (it != reforge_to_string_map.end()) {
            return it->second;
        }
        else {
            throw std::invalid_argument("Unknown Reforge value");
        }
    }
};

struct Gemstone {
    std::string key;
    std::string quality;
    bool isUnlocked;

    bool isValid() const {
        // Define what makes a Gemstone valid
        return !key.empty() && !quality.empty();
    }
};
