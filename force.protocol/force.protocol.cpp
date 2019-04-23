/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include "force.protocol.hpp"

#define ORDER_STATUS_PENDING    0
#define ORDER_STATUS_ACCEPTED   1
#define ORDER_STATUS_COMPLETED  2
#define ORDER_STATUS_LIQUIDATED 3
#define ORDER_STATUS_FORCEREPAYED 4

//@abi action
void loan::inittokens() {

    require_auth(_self);

    tokena.emplace(_self, [&](auto &token) {
        token.contract = N(betdicetoken);
        token.tokens = std::vector<string>{"DICE"};
    });

    tokena.emplace(_self, [&](auto &token) {
        token.contract = N(eosgamecoin2);
        token.tokens = std::vector<string>{"BEST"};
    });

    tokena.emplace(_self, [&](auto &token) {
        token.contract = N(eosmax1token);
        token.tokens = std::vector<string>{"MAX"};
    });

    tokena.emplace(_self, [&](auto &token) {
        token.contract = N(ridlridlcoin);
        token.tokens = std::vector<string>{"RIDL"};
    });

    tokena.emplace(_self, [&](auto &token) {
        token.contract = N(fastwinadmin);
        token.tokens = std::vector<string>{"FAST"};
    });

    tokenb.emplace(_self, [&](auto &token) {
        token.contract = N(eosio);
        token.tokens = std::vector<string>{"EOS"};
    });

    tokenb.emplace(_self, [&](auto &token) {
        token.contract = N(bitpietokens);
        token.tokens = std::vector<string>{"EUSD"};
    });
}

//@abi action
void loan::addtokena(const account_name &contract, const string &sn) {

    require_auth(_self);

    auto token_itr = tokena.find(contract);
    if (token_itr == tokena.end()) {
        tokena.emplace(_self, [&](auto &token) {
            token.contract = contract;
            token.tokens = std::vector<string>{sn};
        });
    } else {
        std::vector<const string>::iterator iter = std::find(token_itr->tokens.begin(), token_itr->tokens.end(), sn);

        eosio_assert(iter == token_itr->tokens.end(), "token already exist");

        tokena.modify(token_itr, _self, [&](auto &token) {
            token.tokens.push_back(sn);
        });
    }
}

//@abi action
void loan::addtokenb(const account_name &contract, const string &sn) {

    require_auth(_self);

    auto token_itr = tokenb.find(contract);
    if (token_itr == tokenb.end()) {
        tokenb.emplace(_self, [&](auto &token) {
            token.contract = contract;
            token.tokens = std::vector<string>{sn};
        });
    } else {
        std::vector<const string>::iterator iter = std::find(token_itr->tokens.begin(), token_itr->tokens.end(), sn);

        eosio_assert(iter == token_itr->tokens.end(), "token already exist");

        tokenb.modify(token_itr, _self, [&](auto &token) {
            token.tokens.push_back(sn);
        });
    }
}

//@abi action
void loan::borrow(const account_name &borrower, const int32_t &lending_cycle, const asset_type &token_a,
                  const asset_type &token_b, const int32_t &pledge_rate, const int32_t &interest_rate,
                  const int32_t &fee_rate,const int64_t &order_id) {

    auto order_itr = orders.find(order_id);
    eosio_assert(order_itr == orders.end(), "order already exists");

    eosio_assert(token_a.asset_.is_valid(), "invalid token-a");
    eosio_assert(token_a.asset_.amount > 0, "token-a must be positive quantity");

    eosio_assert(token_b.asset_.is_valid(), "invalid token-b");
    eosio_assert(token_b.asset_.amount > 0, "token-b must be positive quantity");

    eosio_assert(lending_cycle >= 7, "lending-cycle must be greater than 7 days");

    require_auth(borrower);

    action(
            permission_level{borrower, N(active)},
            token_a.contract, N(transfer),
            std::make_tuple(borrower, _self, token_a.asset_, std::string(""))
    ).send();

    // Store new order
    auto new_order_itr = orders.emplace(borrower, [&](auto &order) {
        order.id = order_id;
        order.borrower = borrower;
        order.lender = N(none);
        order.lending_cycle = lending_cycle;
        order.token_a = token_a;
        order.token_b = token_b;
        order.pledge_rate = pledge_rate;
        order.interest_rate = interest_rate;
        order.fee_rate = fee_rate;
        order.deadline = eosio::time_point_sec(0);
        order.status = ORDER_STATUS_PENDING;
    });
}

//@abi action
void loan::lend(const int64_t &order_id, const account_name &lender, const asset_type &token_b) {
    auto order_itr = orders.find(order_id);
    
    eosio_assert(order_itr != orders.end(), "order not found");

    eosio_assert(lender != order_itr->borrower, "cannot lent to self");

    eosio_assert(token_b.asset_.symbol == order_itr->token_b.asset_.symbol, "attempt to use an invalid type of token");
    eosio_assert(token_b.asset_ >= order_itr->token_b.asset_ + order_itr->token_b.asset_ * order_itr->fee_rate / 10000, "insufficient balance");

    eosio_assert(token_b.contract == order_itr->token_b.contract, "attempt to use an invalid token");


    eosio_assert(order_itr->deadline == eosio::time_point_sec(0) && order_itr->lender == N(none) &&
                 order_itr->status == ORDER_STATUS_PENDING, "order is already accepted by others");

    //TODO: need to check
    auto eos_token = eosio::token(token_b.contract);
    auto eos_token_balance = eos_token.get_balance(lender, order_itr->token_b.asset_.symbol.name());

    eosio_assert(eos_token_balance >= order_itr->token_b.asset_, "overdraw balance");

    require_auth(lender);

    asset token_b_fee = order_itr->token_b.asset_ * order_itr->fee_rate * 2 / 10000;
    asset token_b_to_borrower = order_itr->token_b.asset_ - token_b_fee / 2;

    action(
            permission_level{lender, N(active)},
            order_itr->token_b.contract, N(transfer),
            std::make_tuple(lender, platform, token_b_fee, std::string(""))
    ).send();

    action(
            permission_level{lender, N(active)},
            order_itr->token_b.contract, N(transfer),
            std::make_tuple(lender, order_itr->borrower, token_b_to_borrower, std::string(""))
    ).send();

    orders.modify(order_itr, _self, [&](auto &order) {
        auto deadline = now() + order.lending_cycle * 24 * 60 * 60;//in seconds
        //auto deadline = now() + order.lending_cycle;//TODO::for test
        order.deadline = eosio::time_point_sec(deadline);
        order.lender = lender;
        order.status = ORDER_STATUS_ACCEPTED;
    });
}

//@abi action
void loan::closepstion(const int64_t &order_id, const account_name &by) {
    auto order_itr = orders.find(order_id);
    eosio_assert(order_itr != orders.end(), "order not found");

    eosio_assert(order_itr->status == ORDER_STATUS_ACCEPTED, "invalid operation");

    auto time_now = time_point_sec(now()/*current time*/);

    if (order_itr->deadline > time_now) {
        eosio_assert(by == _self, "only owner of this contract can do this operation before deadline");
    } else {
        eosio_assert(by == order_itr->lender | by == _self,
                     "only lender or owner of this contract can do this operation");
    }

    require_auth(by);

    action(
            permission_level{_self, N(active)},
            order_itr->token_a.contract, N(transfer),
            std::make_tuple(_self, order_itr->lender, order_itr->token_a.asset_, std::string(""))
    ).send();

    orders.modify(order_itr, 0, [&](auto &order) {
        order.status = ORDER_STATUS_LIQUIDATED;
    });
}

//@abi action
void loan::cancelorder(const int64_t &order_id, const account_name &by) {
    auto order_itr = orders.find(order_id);
    eosio_assert(order_itr != orders.end(), "order not found");

    eosio_assert(by == order_itr->borrower || by == _self, "only borrower or contract can do this operation");

    eosio_assert(order_itr->deadline == eosio::time_point_sec(0) && order_itr->lender == N(none) &&
                 order_itr->status == ORDER_STATUS_PENDING, "order is already accepted");

    require_auth(by);

    action(
            permission_level{_self, N(active)},
            order_itr->token_a.contract, N(transfer),
            std::make_tuple(_self, order_itr->borrower, order_itr->token_a.asset_, std::string(""))
    ).send();

    orders.erase(order_itr);
}

//@abi action
void loan::callmargin(const int64_t &order_id, const account_name &borrower, const asset_type &token_a) {
    auto order_itr = orders.find(order_id);
    eosio_assert(order_itr != orders.end(), "order not found");

    eosio_assert(order_itr->status == ORDER_STATUS_ACCEPTED, "operation not allowed");

    eosio_assert(token_a.asset_.symbol == order_itr->token_a.asset_.symbol, "invalid token-a");
    eosio_assert(token_a.asset_.is_valid(), "invalid asset");
    eosio_assert(token_a.asset_.amount > 0, "token-a must be positive quantity");

    require_auth(borrower);

    action(
            permission_level{borrower, N(active)},
            order_itr->token_a.contract, N(transfer),
            std::make_tuple(borrower, _self, token_a.asset_, std::string(""))
    ).send();

    orders.modify(order_itr, 0, [&](auto &order) {
        order.token_a.asset_ += token_a.asset_;
    });
}

//@abi action
void loan::repay(const int64_t &order_id, const account_name &borrower, const asset_type &token_b) {
    auto order_itr = orders.find(order_id);
    eosio_assert(order_itr != orders.end(), "order not found");

    eosio_assert(token_b.asset_.symbol == order_itr->token_b.asset_.symbol, "invalid token-b");
    eosio_assert(order_itr->status == ORDER_STATUS_ACCEPTED, "operation not allowed");

    require_auth(borrower);

    asset total_to_pay = order_itr->token_b.asset_ + order_itr->token_b.asset_ * order_itr->interest_rate * order_itr->lending_cycle / 3650000;
    print("pay", (order_itr->token_b.asset_ * order_itr->interest_rate * order_itr->lending_cycle / 3650000));
    print(" pay", total_to_pay);

    eosio_assert(token_b.asset_ >= total_to_pay, "insufficient balance");

    //repay
    action(
            permission_level{borrower, N(active)},
            order_itr->token_b.contract, N(transfer),
            std::make_tuple(borrower, order_itr->lender, total_to_pay, std::string(""))
    ).send();

    //undelegate
    action(
            permission_level{_self, N(active)},
            order_itr->token_a.contract, N(transfer),
            std::make_tuple(_self, order_itr->borrower, order_itr->token_a.asset_, std::string(""))
    ).send();

    orders.modify(order_itr, _self, [&](auto &order) {
        order.status = ORDER_STATUS_COMPLETED;
    });
}

//@abi action
void loan::forcerepay(const int64_t &order_id, const asset_type &token_a) {
    auto order_itr = orders.find(order_id);
    eosio_assert(order_itr != orders.end(), "order not found");

    eosio_assert(order_itr->status == ORDER_STATUS_ACCEPTED, "invalid operation");

    auto time_now = time_point_sec(now()/*current time*/);

    eosio_assert(order_itr->deadline <= time_now,
                 "operation not allowed before deadline");

    eosio_assert(token_a.asset_.is_valid(), "invalid token-a");

    eosio_assert(token_a.contract == order_itr->token_a.contract, "attempt to use an invalid token");

    eosio_assert(token_a.asset_.amount > 0 && token_a.asset_ <= order_itr->token_a.asset_, "exceed the range of token-a");

    require_auth(_self);

    action(
            permission_level{_self, N(active)},
            order_itr->token_a.contract, N(transfer),
            std::make_tuple(_self, order_itr->lender, token_a.asset_, std::string(""))
    ).send();

    action(
            permission_level{_self, N(active)},
            order_itr->token_a.contract, N(transfer),
            std::make_tuple(_self, order_itr->borrower, order_itr->token_a.asset_- token_a.asset_, std::string(""))
    ).send();

    orders.modify(order_itr, 0, [&](auto &order) {
        order.status = ORDER_STATUS_FORCEREPAYED;
    });
}

EOSIO_ABI(loan, (inittokens)(addtokena)(addtokenb)(borrow)(lend)(closepstion)(cancelorder)(callmargin)(repay)(forcerepay))