#include "bank.h"
#include <string>
#include <utility>

bank::user::user(const std::string &name_) : user_name(name_), balance(100) {
    std::unique_lock l(m);
    transactions.emplace_back(nullptr, 100, "Initial deposit for " + name_);
}

int bank::user::balance_xts() const {
    std::shared_lock l(m);
    return balance;
}

std::string bank::user::name() const noexcept {
    return user_name;
}

bank::user_transactions_iterator bank::user::snapshot_transactions(
    const std::function<void(const std::vector<transaction> &, int)> &f) const {
    std::shared_lock l(m);
    f(transactions, balance);
    return user_transactions_iterator(transactions.size(), this);
}

void bank::user::transfer(bank::user &counterparty,
                          int amount_xts,
                          const std::string &comment) {
    if (this == &counterparty) {
        return;
    }
    std::scoped_lock l(m, counterparty.m);
    if (amount_xts > balance) {
        throw not_enough_funds_error(balance, amount_xts);
    }
    if (amount_xts < 0) {
        throw negative_transaction_error();
    }
    this->cv.notify_all();
    counterparty.cv.notify_all();
    balance -= amount_xts;
    counterparty.balance += amount_xts;
    transactions.emplace_back(&counterparty, -amount_xts, comment);
    counterparty.transactions.emplace_back(this, amount_xts, comment);
}

std::size_t bank::user::get_size() const {
    return transactions.size();
}

bank::transaction bank::user::get_last_transaction(std::size_t i) const {
    return transactions[i];
}

bank::user_transactions_iterator bank::user::monitor() const {
    return user_transactions_iterator(transactions.size(), this);
}

bank::user &bank::ledger::get_or_create_user(const std::string &name) {
    std::unique_lock l(m);
    auto user = users.find(name);
    if (user == users.end()) {
        users.emplace(name, name);
        return users.at(name);
    }
    return user->second;
}

bank::transaction::transaction(const bank::user *counterparty_,
                               int balance_delta_xts_,
                               std::string comment_)
    : counterparty(counterparty_),
      balance_delta_xts(balance_delta_xts_),
      comment(std::move(comment_)) {
}

bank::transaction bank::user_transactions_iterator::wait_next_transaction() {
    std::shared_lock l(my_user->m);
    if (size == my_user->get_size()) {
        my_user->cv.wait(l);
    }
    return my_user->get_last_transaction(size++);
}
bank::user_transactions_iterator::user_transactions_iterator(
    std::size_t const_size,
    const bank::user *my_us)
    : size(const_size), my_user(my_us) {
}
bank::transfer_error::transfer_error(const std::string &message)
    : std::logic_error(message) {
}
bank::not_enough_funds_error::not_enough_funds_error(int available_xts,
                                                     int required_xts)
    : transfer_error("Not enough funds: " + std::to_string(available_xts) +
                     " XTS available, " + std::to_string(required_xts) +
                     " XTS requested") {
}
bank::negative_transaction_error::negative_transaction_error()
    : transfer_error("Required xts is a negative number") {
}