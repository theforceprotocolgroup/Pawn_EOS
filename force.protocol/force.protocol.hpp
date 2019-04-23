/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <utility>
#include <vector>
#include <string>
#include <eosiolib/eosio.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/contract.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/symbol.hpp>
#include <vector>
#include <string>
#include <eosio.token/eosio.token.hpp>
#include <eosiolib/eosio.hpp>

using namespace eosio;

using eosio::indexed_by;
using eosio::const_mem_fun;
using eosio::asset;
using eosio::permission_level;
using eosio::action;
using eosio::print;
using eosio::name;

//@abi table tokeninfoa i64
struct tokena_info {
    account_name contract;
    std::vector<string> tokens;

    uint64_t primary_key() const { return contract; }

    EOSLIB_SERIALIZE(tokena_info, (contract)(tokens))
};

//@abi table tokeninfob i64
struct tokenb_info {
    account_name contract;
    std::vector<string> tokens;

    uint64_t primary_key() const { return contract; }

    EOSLIB_SERIALIZE(tokenb_info, (contract)(tokens))
};

struct asset_type {
    asset asset_;
    account_name contract;

    EOSLIB_SERIALIZE(asset_type, (asset_)(contract))
};

//@abi table order i64
struct order {

    uint64_t id;
    eosio::time_point_sec deadline;
    int8_t status;
    account_name borrower;
    account_name lender;
    //In days
    uint32_t lending_cycle;
    //Token A for pledge
    asset_type token_a;
    //EOS EUSD
    asset_type token_b;
    int32_t pledge_rate;
    int32_t interest_rate;
    int32_t fee_rate;

    uint64_t primary_key() const { return id; }

    EOSLIB_SERIALIZE(order,
                     (id)(deadline)(status)(borrower)(lender)(lending_cycle)(token_a)
                     (token_b)(pledge_rate)(interest_rate)(fee_rate))
};

static const account_name platform = N(xulang111111);

class loan : public eosio::contract {
public:
    loan(account_name self) : contract(self), orders(_self, _self), tokena(_self, _self), tokenb(_self, _self) {}

    /**
      * @brief The table definition, used to store existing orders and their current state
   */
    typedef eosio::multi_index<N(order), order> orders_index;

    orders_index orders;

    typedef eosio::multi_index<N(tokeninfoa), tokena_info> tokena_index;

    /**
       * @brief The table definition, used to store token-a which is supportive
    */
    tokena_index tokena;

    typedef eosio::multi_index<N(tokeninfob), tokenb_info> tokenb_index;

    /**
       * @brief The table definition, used to store token-b which is supportive
    */
    tokenb_index tokenb;

    void inittokens();

    void addtokena(const account_name &contract, const string &sn);

    void addtokenb(const account_name &contract, const string &sn);

    void borrow(const account_name &borrower, const int32_t &lending_cycle, const asset_type &token_a,
                const asset_type &token_b, const int32_t &pledge_rate, const int32_t &interest_rate, const int32_t &fee_rate, const int64_t &order_id);

    void lend(const int64_t &order_id, const account_name &lender, const asset_type &token_b);

    void closepstion(const int64_t &order_id, const account_name &by);

    /**
       * @brief cancel order by borrower or the account of this contract if the order not token or over 7 days
    */
    void cancelorder(const int64_t &order_id, const account_name &by);

    void callmargin(const int64_t &order_id, const account_name &borrower, const asset_type &token_a);

    void repay(const int64_t &order_id, const account_name &borrower, const asset_type &token_b);

    void forcerepay(const int64_t &order_id, const asset_type &token_a);

};