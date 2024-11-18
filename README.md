# HypixelAUFlipper
## Example Output

![Screenshot_19](https://github.com/user-attachments/assets/bf31b9cc-8e98-4455-a434-44c9db7803dc)

# Hypixel Auction Sniper

This program helps you find profitable Hypixel auctions by tracking and analyzing auction data. Once a profitable auction is found, the program will notify you through the console and automatically copy the auction command to your clipboard, allowing you to quickly access and attempt to buy the auction item.

## Features
- Analyzes auction bids in real-time.
- Automatically copies the auction command to your clipboard.
- Calculates average and lowest bin prices.
- Displays auction information such as item tags, starting bids, item tiers, and more.

## Libraries Used
The following libraries were used in the development of this project:
- **cpp-base64-2.rc.08** 
- **curl** 
- **json/include/nlohmann** 
- **libiconv**

## APIs
This project was built using:
- **Coflnet API** for market data analysis.
- **Hypixel API** for fetching auction data.

## Requirements
- Hypixel API Key (You can obtain one by logging into Hypixel and typing `/api new` in the chat).
- C++ compiler.

## Actions
1. Change min profit price:
    ```c++
      float profitvalue = 300000;
    ```
2. Set up and compile the project using your preferred C++ environment (e.g., Visual Studio, g++).

3. Add your Hypixel API key to the program:
    ```c++
    std::string hypixelapikey = "HYPIXEL_API_KEY"; // Replace with your actual Hypixel API key
    ```

### Example Query

You can customize the query to find auctions that meet specific criteria, such as a certain rarity or level, etc... Below is an example of how to query for Legendary pets with level 0:

```c++
// Example query to find Legendary pets with level 0
std::string query = "?query[Rarity]=LEGENDARY&query[PetLevel]=0";
