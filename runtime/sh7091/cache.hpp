#pragma once

namespace sh7091::cache {

void init() __attribute__ ((section (".text.startup.cache_init")));

} // namespace sh7091::cache
