#include <boost/asio.hpp>
#include <fstream>
#include <iostream>
#include "bank.h"

using boost::asio::ip::tcp;

int main([[maybe_unused]] int argc, char *argv[]) {
    try {
        boost::asio::io_context io_context;
        tcp::acceptor acceptor(
            io_context, tcp::endpoint(tcp::v4(), static_cast<unsigned short>(
                                                     std::atoi(*(argv + 1)))));

        std::cout << "Listening at " << acceptor.local_endpoint() << "\n";
        std::cout.flush();
        try {
            std::ofstream ofs(*(argv + 2));
            ofs << acceptor.local_endpoint().port();
            ofs.close();
        } catch (...) {
            std::cerr << "Unable to store port to file " << *(argv + 2) << '\n';
            std::cerr.flush();
        }

        bank::ledger ledger_;

        while (true) {
            auto *client = new tcp::iostream([&]() {
                tcp::socket s = acceptor.accept();
                std::cout << "Connected " << s.remote_endpoint() << " --> "
                          << s.local_endpoint() << "\n";
                std::cout.flush();
                return s;
            }());

            std::thread create_user([&, client = client]() {
                std::string name;
                *client << "What is your name?\n";
                *client >> name;
                auto &n_user = ledger_.get_or_create_user(name);
                *client << "Hi " << name << '\n';

                auto write_one_transaction = [&](const bank::transaction &t) {
                    if (t.counterparty != nullptr) {
                        (*client) << (t.counterparty->name()) << '\t';
                    } else {
                        (*client) << '-' << '\t';
                    }
                    *client << t.balance_delta_xts << '\t' << t.comment << '\n';
                    client->flush();
                };

                auto write_transactions = [&]() {
                    int number_of_transactions = 0;
                    *client >> number_of_transactions;
                    return n_user.snapshot_transactions(
                        [&](const auto &transactions, int balance) {
                            *client << "CPTY\tBAL\tCOMM\n";
                            for (int i = std::max(
                                     0, static_cast<int>(transactions.size()) -
                                            number_of_transactions);
                                 i < static_cast<int>(transactions.size());
                                 i++) {
                                write_one_transaction(transactions[i]);
                            }
                            *client
                                << "===== BALANCE: " + std::to_string(balance) +
                                       " XTS =====\n";
                            client->flush();
                        });
                };
                while (*client) {
                    std::string command;
                    *client >> command;
                    if (command == "balance") {
                        *client << n_user.balance_xts() << '\n';
                    } else if (command == "transactions") {
                        write_transactions();
                    } else if (command == "monitor") {
                        bank::user_transactions_iterator it =
                            write_transactions();
                        while (*client) {
                            write_one_transaction(it.wait_next_transaction());
                        }
                    } else if (command == "transfer") {
                        int value = 0;
                        std::string other_name;
                        *client >> other_name >> value;
                        std::string message;
                        getline(*client, message);
                        [[maybe_unused]] bank::user &other =
                            ledger_.get_or_create_user(other_name);
                        try {
                            n_user.transfer(other, value,
                                            std::string(message.substr(
                                                1, message.size() - 1)));
                            *client << "OK\n";
                            client->flush();
                        } catch (const std::exception &e) {
                            *client << e.what() << '\n';
                        }
                    } else {
                        *client << "Unknown command: '" << command << "'\n";
                    }
                    client->flush();
                }
                std::cout << "Disconnected " + name + '\n';
                std::cout.flush();
            });
            create_user.detach();
        }
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
}