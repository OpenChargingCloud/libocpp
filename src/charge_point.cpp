// SPDX-License-Identifier: Apache-2.0
// Copyright 2020 - 2022 Pionix GmbH and Contributors to EVerest
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <date/date.h>
#include <date/tz.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sys/prctl.h>
#include <thread>

#include <boost/exception/diagnostic_information.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <everest/logging.hpp>

#include <ocpp1_6/types.hpp>

#include <ocpp1_6/charge_point.hpp>

#include <ocpp1_6/database_handler.hpp>

namespace po = boost::program_options;

ocpp1_6::ChargePoint* charge_point;
bool running = false;
std::mutex m;
std::condition_variable cv;

void signal_handler(int sig) {
    if (sig == SIGINT) {
        if (charge_point != nullptr) {
            EVLOG_info << "Stopping websocket connection...";
            charge_point->stop();
            EVLOG_info << "Successfully stopped websocket connection";
            {
                std::lock_guard<std::mutex> lk(m);
                running = false;
            }
            cv.notify_one();
        }
    }
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    po::options_description desc("OCPP charge point");

    desc.add_options()("help,h", "produce help message");
    desc.add_options()("maindir", po::value<std::string>(), "set main dir in which the schemas folder resides");
    desc.add_options()("conf", po::value<std::string>(), "charge point config relative to maindir");
    desc.add_options()("interactive",
                       "provide a simple command line interface to send preconfigured messages to the central system");
    desc.add_options()("logconf", po::value<std::string>(), "The path to a custom logging.ini");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help") != 0) {
        std::cout << desc << "\n";
        return 1;
    }

    std::string maindir = ".";
    if (vm.count("maindir") != 0) {
        maindir = vm["maindir"].as<std::string>();
    }

    // initialize logging as early as possible
    std::string logging_config = maindir + "/logging.ini";
    if (vm.count("logconf") != 0) {
        logging_config = vm["logconf"].as<std::string>();
    }
    Everest::Logging::init(logging_config, "charge_point");

    std::string conf = "config.json";
    if (vm.count("conf") != 0) {
        conf = vm["conf"].as<std::string>();
    }

    bool interactive = false;
    if (vm.count("interactive") != 0) {
        interactive = true;
    }

    auto configs_path = maindir;
    auto schemas_path = maindir;
    auto database_path = "/tmp/ocpp";

    boost::filesystem::path config_path = boost::filesystem::path(maindir) / conf;
    std::ifstream ifs(config_path.c_str());
    std::string config_file((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));

    json json_config = json::parse(config_file);
    auto user_config_path = boost::filesystem::path(configs_path) / "user_config" / "user_config.json";

    if (boost::filesystem::exists(user_config_path)) {

        std::ifstream ifs(user_config_path.c_str());
        std::string user_config_file((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));

        auto user_config = json::parse(user_config_file);

        EVLOG_info << "Augmenting main config with user-config entries";
        json_config.merge_patch(user_config);
    } else {
        EVLOG_debug << "No user-config provided.";
    }

    std::shared_ptr<ocpp1_6::ChargePointConfiguration> configuration =
        std::make_shared<ocpp1_6::ChargePointConfiguration>(json_config, configs_path, schemas_path);


    const boost::filesystem::path sql_init_path = boost::filesystem::path(configs_path) / "init.sql";
    charge_point = new ocpp1_6::ChargePoint(configuration, database_path, sql_init_path.string());
    charge_point->start();

    {
        std::lock_guard<std::mutex> lk(m);
        running = true;
    }

    std::thread t1([&] {
        if (interactive) {
            bool transaction_running = false;
            int32_t value = 0;
            do {
                std::string command;
                std::cin >> command;
                std::cout << std::endl;
                if (command == "auth") {
                    EVLOG_info << "authorize_id_tag command";
                    auto result = charge_point->authorize_id_token(ocpp1_6::CiString20Type(std::string("123")));
                    if (result.status == ocpp1_6::AuthorizationStatus::Accepted) {
                        EVLOG_info << "Authorized";
                    } else {
                        EVLOG_info << "Not authorized";
                    }
                } else if (command == "data_transfer") {
                    EVLOG_info << "data_transfer command";
                    ocpp1_6::DataTransferResponse result =
                        charge_point->data_transfer(ocpp1_6::CiString255Type(std::string("Pionix")),
                                                    ocpp1_6::CiString50Type(std::string("Test")), "Hello there");
                    if (result.status == ocpp1_6::DataTransferStatus::Accepted) {
                        EVLOG_info << "Data transfer accepted";
                    } else {
                        EVLOG_info << "Data transfer not accepted:"
                                   << ocpp1_6::conversions::data_transfer_status_to_string(result.status);
                    }
                } else if (command == "meter") {
                    EVLOG_info << "meter values command";

                    std::map<int32_t, std::vector<ocpp1_6::MeterValue>> meter_values;
                    ocpp1_6::MeterValue val;
                    val.timestamp = ocpp1_6::DateTime();
                    ocpp1_6::SampledValue sample;
                    sample.value = std::to_string(value);
                    EVLOG_info << "sample value: " << sample.value;
                    value += 1;
                    sample.context.emplace(ocpp1_6::ReadingContext::Sample_Periodic);
                    sample.format.emplace(ocpp1_6::ValueFormat::Raw);
                    sample.measurand.emplace(ocpp1_6::Measurand::Energy_Active_Import_Register);
                    sample.phase.emplace(ocpp1_6::Phase::L1);
                    sample.location.emplace(ocpp1_6::Location::Outlet);
                    sample.unit.emplace(ocpp1_6::UnitOfMeasure::Wh);
                    val.sampledValue.push_back(sample);
                    meter_values[0].push_back(val);
                    meter_values[1].push_back(val);
                }  else {
                    EVLOG_info << "unknown command, not doing anything";
                }
            } while (!std::cin.fail() && running);
        } else {
            std::unique_lock<std::mutex> lk(m);
            cv.wait(lk, [] { return !running; });

            lk.unlock();
            cv.notify_one();
        }
    });
    t1.join();

    EVLOG_info << "Goodbye";
    return 0;
}
