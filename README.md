# routex-refresh-client-cxx

C++ refresh client library for [YAXI's](https://yaxi.tech) Open Banking services.

This is the *non-interactive* counterpart to `routex-client-cxx`. Instead of driving an interactive authentication flow (`Credentials`, dialogs, redirects, SCA), it reuses existing *connection data* (established earlier with the interactive client) to call the accounts, balances and transactions services directly. It is intended for background polling, scheduled updates and session refresh.

Its public API lives in the `yaxi::refresh` namespace (the interactive client uses `yaxi`), so both clients can be linked into and used from the same program.

Currently only a Windows distribution is available, shipped as a NuGet package with `x64`, `x86`, and `arm64` runtimes.
Service methods are synchronous and return a `yaxi::Result<T>`, a `std::variant<T, yaxi::Error>` where the error alternative is itself a `std::variant` of typed error structs (see [`include/yaxi/routex-refresh-client.h`](include/yaxi/routex-refresh-client.h)).

See the [documentation](https://docs.yaxi.tech) for the full API reference.

## Installation

Requires Windows, MSVC, and C++17 or newer. Available on NuGet as
[`routex-refresh-client-cxx`](https://www.nuget.org/packages/routex-refresh-client-cxx):

```xml
<!-- packages.config -->
<packages>
  <package id="routex-refresh-client-cxx" version="0.0.1" targetFramework="native" />
</packages>
```

Visual Studio's NuGet Package Manager wires the matching `<Import>` entries into your
`.vcxproj` automatically.

## Usage

```cpp
#include <yaxi/routex-refresh-client.h>

#include <utility>
#include <variant>

using namespace yaxi::refresh;

// std::visit helper
template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

// Unwrap Result<T> to T, throwing the typed error otherwise
template <typename T> T unwrap(Result<T> r) {
    if (auto* e = std::get_if<Error>(&r))
        std::visit([](auto const& err) { throw err; }, *e);
    return std::get<T>(std::move(r));
}

// Pass "https://integration.yaxi.tech" for the integration environment
RoutexRefreshClient client;

// ticket: a YAXI service ticket issued by your backend (see docs)
// connectionData: established earlier with the interactive client
//   (routex-client-cxx) and persisted by your application.

// Fetch accounts non-interactively
auto result = unwrap(client.accounts(
    connectionData,
    accountsTicket,
    { AccountField::Iban, AccountField::Currency, AccountField::OwnerName }
));

// result.json: the decoded service output serialized as JSON (e.g. the array of
//   accounts). Unlike the interactive client, this is NOT a signed JWT — the data
//   is returned already decoded.
// result.session: short-lived, pass to consecutive service calls to speed up
//   authentication.
// result.connectionData: refreshed connection data (when present). Persist it and
//   pass it to the next call so the consent stays usable.
auto next = result.connectionData.value_or(connectionData);

// Reuse the refreshed connection data for the next service
auto balances = unwrap(client.balances(
    next,
    balancesTicket,
    { AccountReference{ /*iban=*/"DE02120300000000202051", /*currency=*/std::string{"EUR"} } }
));
```

The typed errors carried by `yaxi::Error` are documented at [docs.yaxi.tech/errors.html](https://docs.yaxi.tech/errors.html); the full per-service reference lives in the [documentation](https://docs.yaxi.tech).

## License

Apache-2.0
