#include "Api.hpp"
#include <locale>
#include <codecvt>
#include <fstream>
#include <regex>
#include <tchar.h>
#include <sstream>

#include <windows.h>
#include <future>

int profitvalue = 2000000;
int profitvalueExpencive = 4000000;
int profitvalueExpencive2 = 10000000;
API api;
auto lastUpdateCheckTime = std::chrono::steady_clock::now();
int checkingalready = 0;
bool wasupdate = false;


std::string generatePetName(const std::string& tag, const std::string& itemName) {
    // Extract the part of the itemName after the last "] "
    size_t lastBracket = itemName.find_last_of(']');
    std::string name = itemName.substr(lastBracket + 1);

    // Trim leading and trailing whitespace
    name.erase(0, name.find_first_not_of(" \t\n\r"));
    name.erase(name.find_last_not_of(" \t\n\r") + 1);

    // Convert the name to uppercase and replace spaces with underscores
    std::string formattedName;
    std::istringstream nameStream(name);
    std::string word;

    while (nameStream >> word) {
        // Convert each word to uppercase
        std::transform(word.begin(), word.end(), word.begin(), ::toupper);

        // Append the word with underscores between them
        if (!formattedName.empty()) {
            formattedName += "_";
        }
        formattedName += word;
    }

    // Combine the tag and the formatted name with an underscore
    return tag + "_" + formattedName;
}

void CopyToClipboard(const std::string& text) {
    // Open the clipboard
    if (!OpenClipboard(nullptr)) {
        return;
    }

    // Empty the clipboard
    EmptyClipboard();

    // Allocate global memory for the text
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (hGlobal == nullptr) {
        CloseClipboard();
        return;
    }

    // Lock the memory and copy the text to it
    LPVOID pGlobal = GlobalLock(hGlobal);
    if (pGlobal != nullptr) {
        memcpy(pGlobal, text.c_str(), text.size() + 1);
        GlobalUnlock(hGlobal);

        // Set the clipboard data
        SetClipboardData(CF_TEXT, hGlobal);
    }

    // Close the clipboard
    CloseClipboard();
}
// Function to play sound
void playSound(const std::wstring& soundFile) {
    PlaySound(soundFile.c_str(), NULL, SND_FILENAME | SND_ASYNC);
}

int GetVolume(std::vector<ItemPrice> pricehistory)
{
    int volume = 0;
    for (auto item : pricehistory)
    {
        volume += item.volume;
    }
    return volume;
}

bool PetDetect(std::string name,int& level) //[Lvl 1] Baby Yeti
{
    std::regex pattern(R"(\[Lvl (\d+)\] .+)");
    std::smatch match;

    if (std::regex_search(name, match, pattern)) {
        level = std::stoi(match[1].str());
        return true;
    }

    return false;
}

// Function to simulate keyboard input
void SimulateKeyInput(WORD keyCode, bool keyDown) {
    INPUT input = { 0 };
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = keyCode; // Virtual-Key code for the key

    if (!keyDown) {
        input.ki.dwFlags = KEYEVENTF_KEYUP; // Key release
    }

    SendInput(1, &input, sizeof(INPUT));
}

// Function to simulate typing a string (quickly, without delays)
void SimulateFastTyping(const std::string& text) {
    for (char c : text) {
        SHORT key = VkKeyScan(c); // Get virtual key code for the character
        SimulateKeyInput(LOBYTE(key), true);  // Key down
        SimulateKeyInput(LOBYTE(key), false); // Key up
    }
}

// Function to retrieve clipboard text as a string
std::string GetClipboardText() {
    if (!OpenClipboard(nullptr)) return "";

    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData == nullptr) {
        CloseClipboard();
        return "";
    }

    char* pszText = static_cast<char*>(GlobalLock(hData));
    if (pszText == nullptr) {
        GlobalUnlock(hData);
        CloseClipboard();
        return "";
    }

    std::string text(pszText);

    GlobalUnlock(hData);
    CloseClipboard();

    return text;
}

// Function to simulate mouse click at absolute screen coordinates
void SimulateMouseClick(int x, int y) {
    // Set up the INPUT structure
    INPUT input = { 0 };
    input.type = INPUT_MOUSE;

    // Convert screen coordinates to absolute coordinates
    // Windows uses a coordinate system where (0, 0) is top-left and (65535, 65535) is bottom-right of the screen
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    input.mi.dx = (x * 65535) / GetSystemMetrics(SM_CXSCREEN); // X coordinate
    input.mi.dy = (y * 65535) / GetSystemMetrics(SM_CYSCREEN); // Y coordinate
    SendInput(1, &input, sizeof(INPUT));

    // Simulate left mouse button down
    input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    SendInput(1, &input, sizeof(INPUT));

    // Simulate left mouse button up
    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &input, sizeof(INPUT));
}

std::tm GetUTCTime() {
    auto localTime = std::chrono::system_clock::now();
    std::time_t localTimeT = std::chrono::system_clock::to_time_t(localTime);
    std::tm utcTime;
    gmtime_s(&utcTime, &localTimeT);

    return utcTime;
}

long timeDifferenceInSeconds(const std::tm& time1, const std::tm& time2) {
    // Convert hours, minutes, and seconds to seconds
    long timeInSeconds1 = time1.tm_hour * 3600 + time1.tm_min * 60 + time1.tm_sec;
    long timeInSeconds2 = time2.tm_hour * 3600 + time2.tm_min * 60 + time2.tm_sec;

    // Calculate the absolute difference in seconds
    return std::abs(timeInSeconds2 - timeInSeconds1);
}

long lastdif = 0;

bool ShouldUpdate(std::tm UpdateTime)
{
    std::tm currentTime = GetUTCTime();
    long dif = timeDifferenceInSeconds(UpdateTime,currentTime);

    //std::cout <<  "dif: " << dif << std::endl;
    if (lastdif > dif) { lastdif = dif; return true; }
    lastdif = dif;
    return false;
}

std::string formatPriceDifference(double price) {
    if (price >= 1000000) {
        // If the price is >= 1 million, format it with "m" for million
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << price / 1000000 << "m";
        return ss.str();
    }
    else if (price >= 1000) {
        // If the price is >= 1 thousand, format it with "k" for thousand
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << price / 1000 << "k";
        return ss.str();
    }
    else {
        // Otherwise, just return the price as is
        std::stringstream ss;
        ss << price;
        return ss.str();
    }
}

void AutoNavigate()
{

    SimulateKeyInput(VK_OEM_2, true);  // "/" key down
    SimulateKeyInput(VK_OEM_2, false); // "/" key up

    Sleep(50); // Small delay

    SimulateKeyInput(VK_BACK, true);   // Backspace down
    SimulateKeyInput(VK_BACK, false);  // Backspace up

    Sleep(50); // Small delay

    std::string clipboardText = GetClipboardText();
    if (!clipboardText.empty()) {
        SimulateFastTyping(clipboardText);
    }

    Sleep(300); // Small delay

    SimulateKeyInput(VK_RETURN, true);  // Enter key down
    SimulateKeyInput(VK_RETURN, false); // Enter key up

    Sleep(1000);
    SimulateMouseClick(959, 440);
    Sleep(500);
    SimulateMouseClick(851, 422);
}

void ProcessAuctions(Auction auction, std::atomic<bool>& stopFlag)
{

    if (stopFlag.load()) return; // Check if we should stop processing

    if (auction.claimed) return;

    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdateCheckTime);

    if (duration.count() > 1) {  // Check every second (you can adjust this interval)
        checkingalready += 1;
        if (checkingalready <= 1)
        {
            if (ShouldUpdate(api.GetAuctionLastTimeUpdate())) {
                stopFlag.store(true); // Signal to stop further processing
                checkingalready = 0;
                return;
            }
            checkingalready = 0;
            lastUpdateCheckTime = now;  // Update the time of last check
        }
    }

    std::string queryParams;
    int auctionitemstar = 0;
    int petlevel = 0;
    bool ispet = PetDetect(auction.item_name, petlevel);

    if (ispet) {
        queryParams = "?query[Rarity]=" + auction.tier + "&query[Stars]=" + std::to_string(auctionitemstar) + "&query[PetLevel]=" + std::to_string(petlevel);
    }
    else {
        queryParams = "?query[Rarity]=" + auction.tier + "&query[Stars]=" + std::to_string(auctionitemstar);
    }


    // std::cout << "Auction information retrieved successfully.\n";
    //std::vector<AuctionItem> soldedsorted;
    //std::future<bool> SoldCheckRes = std::async(std::launch::async, CheckSoldedItems, auctionitem, std::ref(soldedsorted), auction.tier,ispet, petlevel);

    std::vector<AuctionItem> itemprices;
    std::vector<ItemPrice> pricehistory;
    if (ispet)
        auction.auctionitem.id_in_attributes = generatePetName(auction.auctionitem.id_in_attributes, auction.item_name);
    api.GetLastsAuctionLowestPrices(auction.auctionitem.id_in_attributes, itemprices, queryParams);
    if (itemprices.size() == 0) { Sleep(10000);   api.GetLastsAuctionLowestPrices(auction.auctionitem.id_in_attributes, itemprices, queryParams); };
   // std::cout << "prices size: " << itemprices.size() << std::endl;
    if (itemprices.size() < 2) return;
    float lastprice = itemprices[0].startingBid;
    float secondlastprice = itemprices[1].startingBid;

    float profitLast = lastprice - auction.starting_bid;
    float profitSecondLast = secondlastprice - auction.starting_bid;
    if (profitLast <= profitvalue || profitSecondLast <= profitvalue) return;
    api.GetItemPriceHistory(auction.auctionitem.id_in_attributes, pricehistory, "/week", queryParams);
    if (itemprices.empty()) return;
    if (pricehistory.empty()) return;


    int volumeday = GetVolume(pricehistory);
    int volumedayaverage = ItemPrice::CalculateFilteredAverageStartingBid(pricehistory);



    if (auctionitemstar == 0 && !ispet) {
        if (volumeday < 500) return;
        if (volumedayaverage < 5) return;
    }
    if(auction.starting_bid > 50000000)
        if ((profitLast <= profitvalueExpencive2 || profitSecondLast <= profitvalueExpencive2) || !auction.bids.empty()) return;
    if (auction.starting_bid > 20000000)
        if ((profitLast <= profitvalueExpencive || profitSecondLast <= profitvalueExpencive) || !auction.bids.empty()) return;
    if ((profitLast <= profitvalue || profitSecondLast <= profitvalue) || !auction.bids.empty()) return;

    if (auction.tier != "COMMON" && auction.tier != "UNCOMMON") {
        std::cout << std::fixed << "name: " << auction.item_name << " price: " << auction.starting_bid << " profitLast: " << std::setprecision(2) << profitLast << " uuid: " << auction.uuid << std::endl;
        std::cout << std::fixed << "\033[32m" << "pricedifference1: " << std::setprecision(2) << formatPriceDifference(profitLast) << "\033[0m" << std::endl;
        CopyToClipboard("/viewauction " + auction.uuid);
        playSound(L"news-ting-6832.wav");
       // AutoNavigate(); // dont do it or ban ;-;
    }
}

void AlgoWIthAsynk()
{
    int page = 0;
    // Create a ThreadPool with the number of hardware threads
    ThreadPool pool(std::thread::hardware_concurrency());
    std::atomic<bool> stopProcessing(false);
    int auctioncount = 0;

    // Main loop to process auctions
    while (true) {
        std::vector<Auction> auctions;

            api.GetAuctions(auctions, page);
     
        std::cout << "auctions: " << auctions.size() << " page: " << page << "\n";

        // Reset stop flag for the new batch
        stopProcessing.store(false);

        // Enqueue tasks to the thread pool
        std::vector<std::future<void>> futures;
        for (const auto& auction : auctions) {
            if (futures.size() > 80) break;
            futures.push_back(pool.enqueue(ProcessAuctions, auction, std::ref(stopProcessing)));
        }

        // Wait for all tasks to complete or stop early if flagged
        for (auto& future : futures) {
            future.get();
            if (stopProcessing.load()) { wasupdate = true; break; } // Stop processing further if flagged
        }

        // Wait until auction data is updated
        while (true) {
            if (ShouldUpdate(api.GetAuctionLastTimeUpdate()))
                break;
            if (wasupdate) break;
            Sleep(500);
        }
        wasupdate = false;
    }
    return;
}

int main()
{
    AlgoWIthAsynk();
}
