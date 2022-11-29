#include <Geode/loader/Hook.hpp>
#include <Geode/loader/Loader.hpp>
#include <Geode/loader/Mod.hpp>
#include <Geode/utils/casts.hpp>
#include <Geode/utils/ranges.hpp>
#include <vector>
// #include <hook/hook.hpp>
#include "InternalLoader.hpp"
#include "InternalMod.hpp"

#include <Geode/hook-core/Hook.hpp>

USE_GEODE_NAMESPACE();

struct hook_info {
    Hook* hook;
    Mod* mod;
};

// for some reason this doesn't work as
// a normal static global. the vector just
// gets cleared for no reason somewhere
// between `addHook` and `loadHooks`

GEODE_STATIC_VAR(std::vector<hook_info>, internalHooks);
GEODE_STATIC_VAR(bool, readyToHook);

Result<> Mod::enableHook(Hook* hook) {
    if (!hook->isEnabled()) {
        auto res = std::invoke(hook->m_addFunction, hook->m_address);
        if (res) {
            log::debug("Enabling hook at function {}", hook->m_displayName);
            this->m_hooks.push_back(hook);
            hook->m_enabled = true;
            hook->m_handle = res.unwrap();
            return Ok();
        }
        else {
            return Err(
                "Unable to create hook at " +
                std::to_string(reinterpret_cast<uintptr_t>(hook->m_address))
            );
        }
        return Err("Hook already has a handle");
    }
    return Ok();
}

Result<> Mod::disableHook(Hook* hook) {
    if (hook->isEnabled()) {
        if (geode::core::hook::remove(hook->m_handle)) {
            log::debug("Disabling hook at function {}", hook->m_displayName);
            hook->m_enabled = false;
            return Ok();
        }
        return Err("Unable to remove hook");
    }
    return Ok();
}

Result<> Mod::removeHook(Hook* hook) {
    auto res = this->disableHook(hook);
    if (res) {
        ranges::remove(m_hooks, hook);
        delete hook;
    }
    return res;
}

Result<Hook*> Mod::addHook(Hook* hook) {
    if (readyToHook()) {
        auto res = this->enableHook(hook);
        if (!res) {
            delete hook;
            return Err("Can't create hook");
        }
    }
    else {
        internalHooks().push_back({ hook, this });
    }
    return Ok<Hook*>(hook);
}

bool InternalLoader::loadHooks() {
    readyToHook() = true;
    auto thereWereErrors = false;
    for (auto const& hook : internalHooks()) {
        auto res = hook.mod->addHook(hook.hook);
        if (!res) {
            log::log(Severity::Error, hook.mod, "{}", res.unwrapErr());
            thereWereErrors = true;
        }
    }
    // free up memory
    internalHooks().clear();
    return !thereWereErrors;
}

nlohmann::json Hook::getRuntimeInfo() const {
    auto json = nlohmann::json::object();
    json["address"] = reinterpret_cast<uintptr_t>(m_address);
    json["detour"] = reinterpret_cast<uintptr_t>(m_detour);
    json["name"] = m_displayName;
    json["enabled"] = m_enabled;
    return json;
}
