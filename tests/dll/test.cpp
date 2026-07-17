#include "pch.h"

#include <yaxi/routex-refresh-client.h>

#include <optional>
#include <string>
#include <variant>

using namespace yaxi::refresh;

template <typename T> T unwrap(yaxi::refresh::Result<T> result) {
    if (const auto error = std::get_if<yaxi::refresh::Error>(&result)) {
        std::visit([](const auto &e) { throw e; }, *error);
    }

    return std::get<T>(std::move(result));
}

extern "C" {
__declspec(dllexport) size_t search(const char *url_c, const char *ticket_c) {
    std::string url(url_c);
    std::string ticket(ticket_c);
    RoutexRefreshClient client(url);

    std::vector<yaxi::refresh::SearchFilter> filters{yaxi::refresh::TermSearchFilter{"sparkasse"},
                                                     yaxi::refresh::TermSearchFilter{"stadt"}};

    std::vector<yaxi::refresh::ConnectionInfo> connectionInfos =
        unwrap(client.search(ticket, filters, true, 20));

    return connectionInfos.size();
}
}
