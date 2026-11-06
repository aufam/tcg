#include <mup/html.h>
#include "main.h"

import fmt;

using namespace mup::html;

std::string CleanModal() {
    return div_<(id_ = "modal", class_ = "modal", hx_swap_oob = "true")>{};
}
