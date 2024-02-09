#include <vector>
#include <map>
#include <string>
#include <optional>
#include <sstream>
#include <tuple>
#include <algorithm>
#include <iostream>

class MatchEngine {
    
    public:
    

        MatchEngine(std::vector<std::string> const & input) {
            this -> _input = input;
            _BOOK.resize(2);
        };
        
        std::vector<std::string> getOutputList(void) {
            
            for (const auto& item: _input) {

                auto res = this -> isValidInput(item);
                if (!res.has_value())
                    continue;
                
                auto& [command, order] = res.value();
                
                (this->*MatchEngine::commands[command])(order);
            }

            this -> pairTrade();

            return this -> _finalOutput;
        };
    
        enum SIDE {
            BUY,
            SELL,
            NONE
        };
        
        enum COMMAND {
            INSERT,
            AMEND,
            PULL,
        };
        


        static const char BUY_STR[];
        static const char INSERT_STR[];
        static const char AMEND_STR[];


    private:
        struct Order {
            int id = -1;
            enum MatchEngine::SIDE side = MatchEngine::SIDE::NONE;
            int volume = INT32_MAX;
            std::string symbol = "";
            double price = 0;

        };

        /*Define Pointer to method*/
        typedef void (MatchEngine::*Command)(Order&);

        /*Base Array Input*/
        std::vector<std::string> _input;
        /*Vector that will hold all BUY trades at INDEX 0, And all Sell trades at index 1*/
        std::vector<std::vector<MatchEngine::Order>> _BOOK;
        /*Vector that will hold the end result*/
        std::vector<std::string> _finalOutput;
        /*Map that will hold by symbol all leftover trades*/
        std::map<std::string, std::unordered_map<size_t, std::map<double, MatchEngine::Order>>> _leftTrades;
        /*Array that will contain pointer to method: insert, amend and pull*/
        Command commands[3] = {
            &MatchEngine::insert,
            &MatchEngine::amend,
            &MatchEngine::pull
        };


        /*That function will get all leftover trade and pair them up based on the best matchup*/
        void pairTrade() {

            for (size_t i = 0; i < _BOOK.size(); i++)
            {
                auto& book = _BOOK[i];

                for (size_t j = 0; j < book.size(); j++)
                {
                    auto& map = _leftTrades[book[j].symbol][i]; 
                    auto it = map.find(book[j].price);
                    
                    if (it == map.end()) {
                        map[book[j].price] = book[j];
                        continue;
                    }

                    it -> second.volume += book[j].volume;
                }

            }
            

            for (auto& [symbol, book]: _leftTrades) {
                
                this -> _finalOutput.emplace_back("===" + symbol + "===");
                auto& buyBook = book[MatchEngine::SIDE::BUY];
                auto& sellBook = book[MatchEngine::SIDE::SELL];
                
                auto sellIt = sellBook.begin();

                for (auto buyIt = buyBook.rbegin(); buyIt != buyBook.rend(); ++buyIt) {

                    if (sellIt != sellBook.end()) {
                        this -> _finalOutput
                        .emplace_back(registerLeftTrade(buyIt->second.price, buyIt->second.volume, sellIt->second.price, sellIt->second.volume));
                        sellIt++;
                        continue;
                    }

                    this -> _finalOutput.emplace_back(registerLeftTrade(buyIt -> second.price, buyIt -> second.volume, MatchEngine::SIDE::BUY));
                }

                for (; sellIt  != sellBook.end(); sellIt++)
                {
                    this -> _finalOutput.emplace_back(registerLeftTrade(sellIt -> second.price, sellIt -> second.volume, MatchEngine::SIDE::SELL));
                }
                              
            }

        }

        /*Check For an Order based on a given Id, and pull it out from the book if present in one of them*/
        void pull(Order& orderToDelete) {
            for (auto& book: _BOOK) {
                
                auto it = std::find_if(book.begin(), book.end(), [&](auto& order) {
                    return order.id == orderToDelete.id;
                });
                
                if (it != book.end()) {
                    book.erase(it);
                    return ;
                }
            }
        }

        /*Amend Order If Found in one of the two books*/
        void amend(Order& orderToUpdate) {
            for (auto& book: _BOOK) {
                
                auto it = std::find_if(book.begin(), book.end(), [&](auto& order) {
                    return order.id == orderToUpdate.id;
                });
                
                if (it != book.end()) {
            
                    if (it -> price == orderToUpdate.price && it -> volume > orderToUpdate.volume) {
                        it ->  volume = orderToUpdate.volume;
                    }
                    else {
                        Order newOrder(*it);
                        newOrder.volume = orderToUpdate.volume;
                        book.erase(it);
                        if (newOrder.price != orderToUpdate.price) {
                            newOrder.price = orderToUpdate.price;
                            this -> insert(newOrder);
                        }
                        else {
                            book.emplace_back(newOrder);
                        }
                    }
                    
                    return;
                }
            }
        }
        

        /*This Method will handle the insert of new order in their respectives books*/
        void insert(Order& newOrder) {

            int side = newOrder.side;
            int remainingVolume = newOrder.volume;
            auto& myOrderBook = _BOOK[newOrder.side];
            auto& book = _BOOK[side == MatchEngine::SIDE::BUY ? MatchEngine::SIDE::SELL : MatchEngine::SIDE::BUY];

            for (size_t i = 0; i < book.size(); i++) {
                    
                if (book[i].symbol == newOrder.symbol
                    && ((side == MatchEngine::SIDE::BUY && book[i].price <= newOrder.price)
                    || (side == MatchEngine::SIDE::SELL && book[i].price >= newOrder.price))) {

                    int tradedVolume = remainingVolume >= book[i].volume ? book[i].volume : remainingVolume;
                    remainingVolume -= book[i].volume;
                    book[i].volume -= tradedVolume;
                    _finalOutput.emplace_back(this -> registerTrade(book[i].symbol, book[i].price, tradedVolume, newOrder.id, book[i].id));
                        
                    if (book[i].volume == 0) {
                        book.erase(book.begin() + i);
                        --i;
                    }
                            
                    if (remainingVolume <= 0) return ;
                }
            }
                
            newOrder.volume = remainingVolume;
            myOrderBook.emplace_back(newOrder);
        }

        /*Function that will given leftover trades data, return back a formated string expected for the outputlist as such
        <bid_price>,<bid_volume>,<ask_price>,<ask_volume>*/
        std::string registerLeftTrade(const double& bid_price, const int& bid_volume, const double& ask_price, const int& ask_volume) {
            std::ostringstream s_bid, s_ask;
            s_bid << bid_price;
            s_ask << ask_price;
            return s_bid.str() + ',' + std::to_string(bid_volume) + ',' + s_ask.str() + ',' + std::to_string(ask_volume);
        }

        std::string registerLeftTrade(const double& price, const int& volume, MatchEngine::SIDE side) {
            std::ostringstream ss;
            ss << price;
            
            if (side == MatchEngine::SIDE::BUY) return ss.str() + ',' + std::to_string(volume) + ",,";
            
            return ",," + ss.str() + ',' + std::to_string(volume);
        }

        /*Function that will given a matched traded data, return back a formated string expected for the outputlist as such
        <symbol>,<price>,<volume>,<aggressive_order_id>,<passive_order_id>*/
        std::string registerTrade(const std::string& symbol, const double& price, const int& volume, const int& a_id, const int& p_id) {
            std::ostringstream strs;
            strs << price;
            return symbol + "," + strs.str() + "," + std::to_string(volume) + "," + std::to_string(a_id) + "," + std::to_string(p_id);   
        }

        
        /*Method that will check if the given input command has valid syntax, if so a vector holding all the relevant data will be returned, and if not nullopt will be returned*/
        std::optional<std::tuple<MatchEngine::COMMAND, MatchEngine::Order>> isValidInput(const std::string& command) {
            
            std::vector<std::string> vec;
            std::stringstream ss(command);
            std::string data;
            
            while (std::getline(ss, data, ','))
                vec.emplace_back(data);
            
            enum MatchEngine::COMMAND action;
            
            MatchEngine::Order order;
            
            order.id = std::stoi(vec[1]);
            
            if (vec[0] == MatchEngine::INSERT_STR) {
                action = MatchEngine::COMMAND::INSERT;
                order.symbol = vec[2];
                order.side = vec[3] == BUY_STR ? MatchEngine::SIDE::BUY : MatchEngine::SIDE::SELL;
                order.price = std::stod(vec[4]);
                order.volume = std::stoi(vec[5]);
            }
            else if (vec[0] == MatchEngine::AMEND_STR) {
                action = MatchEngine::COMMAND::AMEND;
                order.price = std::stod(vec[2]);
                order.volume = std::stoi(vec[3]);
            }
            else {
                action = MatchEngine::COMMAND::PULL;
            }
            
            size_t priceIndex = vec.size() - 2;

            size_t commaPos = vec[priceIndex].rfind('.');
            
            if (action != MatchEngine::COMMAND::PULL
                && commaPos != std::string::npos && vec[priceIndex].size() - 1 - commaPos > 4)
                return std::nullopt;
            return std::make_tuple(action, order);
        }
};

const char MatchEngine::BUY_STR[] = "BUY";
const char MatchEngine::INSERT_STR[] = "INSERT";
const char MatchEngine::AMEND_STR[] = "AMEND";

std::vector<std::string> run(std::vector<std::string> const& input) {
    MatchEngine matchingEngine(input);
    
    return matchingEngine.getOutputList();
}

std::ostream& operator<<(std::ostream& os, const std::vector<std::string>& vec)
{
    for (const auto& item: vec) {
        std:: cout << item << '\n';
    }
    return os;
}

int main () {
    auto input = std::vector<std::string>();

input.emplace_back("INSERT,1,AAPL,BUY,12.2,5");
    auto res = run(input);
    std::cout << res;
}