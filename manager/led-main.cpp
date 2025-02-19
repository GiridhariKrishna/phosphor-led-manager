#include "config.h"

#include "group.hpp"
#include "ledlayout.hpp"
#ifdef LED_USE_JSON
#include "json-parser.hpp"
#else
#include "led-gen.hpp"
#endif
#include "manager.hpp"
#include "serialize.hpp"
#include "utils.hpp"
#ifdef USE_LAMP_TEST
#include "lamptest/lamptest.hpp"
#endif

#include <CLI/CLI.hpp>
#include <sdeventplus/event.hpp>

#include <algorithm>
#include <iostream>

int main(int argc, char** argv)
{
    CLI::App app("phosphor-led-manager");

#ifdef LED_USE_JSON
    std::string configFile{};
    app.add_option("-c,--config", configFile, "Path to JSON config");
#endif

    CLI11_PARSE(app, argc, argv);

    // Get a default event loop
    auto event = sdeventplus::Event::get_default();

    /** @brief Dbus constructs used by LED Group manager */
    auto& bus = phosphor::led::utils::DBusHandler::getBus();

#ifdef LED_USE_JSON
    auto systemLedMap = getSystemLedMap(configFile);
#endif

    /** @brief Group manager object */
    phosphor::led::Manager manager(bus, systemLedMap);

    /** @brief sd_bus object manager */
    sdbusplus::server::manager_t objManager(bus, OBJPATH);

    /** @brief vector of led groups */
    std::vector<std::unique_ptr<phosphor::led::Group>> groups;

    /** @brief store and re-store Group */
    phosphor::led::Serialize serialize(SAVED_GROUPS_FILE);

#ifdef USE_LAMP_TEST
    phosphor::led::LampTest lampTest(event, manager);

    groups.emplace_back(std::make_unique<phosphor::led::Group>(
        bus, LAMP_TEST_OBJECT, manager, serialize,
        std::bind(std::mem_fn(&phosphor::led::LampTest::requestHandler),
                  &lampTest, std::placeholders::_1, std::placeholders::_2)));

    // Register a lamp test method in the manager class, and call this method
    // when the lamp test is started
    manager.setLampTestCallBack(
        std::bind(std::mem_fn(&phosphor::led::LampTest::processLEDUpdates),
                  &lampTest, std::placeholders::_1, std::placeholders::_2));
#endif

    /** Now create so many dbus objects as there are groups */
    std::ranges::transform(systemLedMap, std::back_inserter(groups),
                           [&bus, &manager, &serialize](auto& grp) {
                               return std::make_unique<phosphor::led::Group>(
                                   bus, grp.first, manager, serialize);
                           });

    // Attach the bus to sd_event to service user requests
    bus.attach_event(event.get(), SD_EVENT_PRIORITY_NORMAL);

    /** @brief Claim the bus */
    bus.request_name(BUSNAME);
    event.loop();

    return 0;
}
