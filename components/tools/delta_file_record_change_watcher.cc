// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <future>
#include <iostream>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "components/data/realtime/delta_file_record_change_notifier.h"
#include "components/util/platform_initializer.h"
#include "public/constants.h"
#include "public/data_loading/data_loading_generated.h"
#include "public/data_loading/filename_utils.h"
#include "public/data_loading/readers/riegeli_stream_io.h"

ABSL_FLAG(std::string, sns_arn, "", "sns_arn");

using kv_server::DeltaFileRecordChangeNotifier;

void Print(std::string string_decoded) {
  std::istringstream is(string_decoded);

  auto delta_stream_reader_factory =
      kv_server::StreamRecordReaderFactory<std::string_view>::Create();

  auto record_reader = delta_stream_reader_factory->CreateReader(is);

  auto result = record_reader->ReadStreamRecords([](std::string_view raw) {
    auto record = flatbuffers::GetRoot<kv_server::DeltaFileRecord>(raw.data());

    auto recordVerifier = flatbuffers::Verifier(
        reinterpret_cast<const uint8_t*>(raw.data()), raw.size());

    if (!record->Verify(recordVerifier)) {
      std::cerr << "Record flatbuffer format is not valid." << std::endl;
      return absl::InvalidArgumentError(
          "Record flatbuffer format is not valid.");
    }

    auto update_type = "update";
    switch (record->mutation_type()) {
      case kv_server::DeltaMutationType::Delete: {
        update_type = "delete";
        break;
      }
    }

    std::cout << "key: " << record->key()->string_view() << std::endl;
    std::cout << "value: " << record->value()->string_view() << std::endl;
    std::cout << "logical_commit_time: " << record->logical_commit_time()
              << std::endl;
    std::cout << "update_type: " << update_type << std::endl;
    return absl::OkStatus();
  });
}

int main(int argc, char** argv) {
  kv_server::PlatformInitializer initializer;

  std::vector<char*> commands = absl::ParseCommandLine(argc, argv);
  std::string sns_arn = absl::GetFlag(FLAGS_sns_arn);
  if (sns_arn.empty()) {
    std::cerr << "Must specify sns_arn" << std::endl;
    return -1;
  }

  auto message_service_status = kv_server::MessageService::Create(
      kv_server::CloudNotifierMetadata{"BlobNotifier_", sns_arn});

  if (!message_service_status.ok()) {
    std::cerr << "Unable to create MessageService: "
              << message_service_status.status().message();
    return -1;
  }

  auto status_or_notifier =
      kv_server::ChangeNotifier::Create(kv_server::CloudNotifierMetadata{
          .queue_prefix = "QueueNotifier_",
          .sns_arn = std::move(sns_arn),
          .queue_manager = message_service_status->get()});

  if (!status_or_notifier.ok()) {
    std::cerr << "Unable to create ChangeNotifier: "
              << status_or_notifier.status().message();
    return -1;
  }

  std::unique_ptr<DeltaFileRecordChangeNotifier> notifier =
      DeltaFileRecordChangeNotifier::Create(std::move(*status_or_notifier));

  while (true) {
    auto keys = notifier->GetNotifications(absl::InfiniteDuration(),
                                           []() { return false; });
    if (keys.ok()) {
      for (const auto& key : keys->parsed_notifications) {
        Print(key);
      }
    } else {
      std::cerr << keys.status() << std::endl;
    }
  }
  return 0;
}
