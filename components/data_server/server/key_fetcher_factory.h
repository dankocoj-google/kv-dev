/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <memory>

#ifndef COMPONENTS_DATA_SERVER_SERVER_KEY_FETCHER_FACTORY_H_
#define COMPONENTS_DATA_SERVER_SERVER_KEY_FETCHER_FACTORY_H_

#include "components/data_server/server/parameter_fetcher.h"
#include "src/cpp/encryption/key_fetcher/interface/key_fetcher_manager_interface.h"

namespace kv_server {

// Constructs a KeyFetcherManager instance.
std::unique_ptr<privacy_sandbox::server_common::KeyFetcherManagerInterface>
CreateKeyFetcherManager(const ParameterFetcher& parameter_fetcher);

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_SERVER_KEY_FETCHER_FACTORY_H_