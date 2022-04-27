#pragma once

#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

namespace bank {
struct transfer_error : std::logic_error {
    explicit transfer_error(const std::string &message);
};

struct not_enough_funds_error : transfer_error {
    not_enough_funds_error(int available_xts, int required_xts);
};

struct negative_transaction_error : transfer_error {
    negative_transaction_error();
};

struct transaction;

struct user;

struct user_transactions_iterator {
private:
    std::size_t size;
    const user *my_user;

public:
    transaction wait_next_transaction();

    explicit user_transactions_iterator() = default;

    user_transactions_iterator(user_transactions_iterator &&) = default;

    user_transactions_iterator(const user_transactions_iterator &) = default;

    user_transactions_iterator &operator=(const user_transactions_iterator &) =
        default;

    user_transactions_iterator &operator=(user_transactions_iterator &&) =
        default;

    ~user_transactions_iterator() = default;

    user_transactions_iterator(std::size_t const_size, const user *my_us);
};

struct user {
    // NOLINTNEXTLINE
    mutable std::shared_mutex m;
    // NOLINTNEXTLINE
    mutable std::condition_variable_any cv;

private:
    const std::string user_name;
    int balance{};
    std::vector<transaction> transactions;

public:
    explicit user(const std::string &name_);

    [[nodiscard]] int balance_xts() const;

    [[nodiscard]] std::string name() const noexcept;

    user_transactions_iterator snapshot_transactions(
        const std::function<void(const std::vector<transaction> &, int)> &f)
        const;

    void transfer(user &counterparty,
                  int amount_xts,
                  const std::string &comment);

    user_transactions_iterator monitor() const;

    std::size_t get_size() const;

    transaction get_last_transaction(std::size_t i) const;
};

struct transaction {
    // NOLINTNEXTLINE
    const user *const counterparty;
    // NOLINTNEXTLINE
    const int balance_delta_xts;
    // NOLINTNEXTLINE
    const std::string comment;

    transaction(const user *counterparty_,
                int balance_delta_xts_,
                std::string comment_);
};

struct ledger {
private:
    std::map<std::string, user> users;
    std::mutex m;

public:
    user &get_or_create_user(const std::string &name);
};

}  // namespace bank
