#pragma once
#include "../core/config.h"
#include "provider_client.h"
#include <vector>

// Fetch usage for all enabled providers whose interval elapsed.
// Returns true if anything changed (screen needs redraw).
bool providersRefresh(bool force = false);

// Client (strategy + visual identity) for a provider type; null when no
// client is registered for it (e.g. "push").
ProviderClient *providerClientFor(const String &type);

// All registered clients — the single source of provider knowledge.
const std::vector<ProviderClient *> &providerClients();

// Like providerClientFor, but client-less providers (push) get a generic
// base-behavior client instead of null — always safe to dispatch on.
ProviderClient &providerClientOrGeneric(const Provider &p);
