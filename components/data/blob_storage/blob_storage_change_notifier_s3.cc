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

#include "absl/functional/bind_front.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "aws/core/utils/json/JsonSerializer.h"
#include "components/data/blob_storage/blob_storage_change_notifier.h"
#include "components/data/common/change_notifier.h"
#include "glog/logging.h"

namespace kv_server {
namespace {

class S3BlobStorageChangeNotifier : public BlobStorageChangeNotifier {
 public:
  explicit S3BlobStorageChangeNotifier(std::string sns_arn)
      : change_notifier_(ChangeNotifier::Create("BlobNotifier_", sns_arn)) {}

  absl::StatusOr<std::vector<std::string>> GetNotifications(
      absl::Duration max_wait,
      const std::function<bool()>& should_stop_callback) override {
    auto notifications =
        change_notifier_->GetNotifications(max_wait, should_stop_callback);

    if (!notifications.ok()) {
      return notifications.status();
    }

    std::vector<std::string> parsed_notifications;
    for (const auto& message : *notifications) {
      const absl::StatusOr<std::string> parsedMessage =
          ParseObjectKeyFromJson(message);
      if (!parsedMessage.ok()) {
        LOG(ERROR) << "Failed to parse JSON: " << message;
        continue;
      }
      parsed_notifications.push_back(std::move(*parsedMessage));
    }
    return parsed_notifications;
  }

 private:
  // TODO: b/267770398 -- switch to nlohmann/json
  absl::StatusOr<std::string> ParseObjectKeyFromJson(
      const std::string& message_body) {
    Aws::Utils::Json::JsonValue json(message_body);
    if (!json.WasParseSuccessful()) {
      return absl::InvalidArgumentError(json.GetErrorMessage());
    }
    Aws::Utils::Json::JsonView view = json;
    const auto message = view.GetObject("Message");
    if (!message.IsString()) {
      return absl::InvalidArgumentError("Message is not a string");
    }
    Aws::Utils::Json::JsonValue message_json(message.AsString());
    if (!message_json.WasParseSuccessful()) {
      return absl::InvalidArgumentError(message_json.GetErrorMessage());
    }
    view = message_json;

    const auto records = view.GetObject("Records");
    if (!records.IsListType()) {
      return absl::InvalidArgumentError("Records not list");
    }
    if (records.AsArray().GetLength() < 1) {
      return absl::InvalidArgumentError("No records found");
    }
    const auto s3 = records.AsArray()[0].GetObject("s3");
    if (s3.IsNull()) {
      return absl::InvalidArgumentError("s3 is null");
    }
    const auto object = s3.GetObject("object");
    if (object.IsNull()) {
      return absl::InvalidArgumentError("object is null");
    }
    const auto key = object.GetObject("key");
    if (!key.IsString()) {
      return absl::InvalidArgumentError("key not string");
    }
    return key.AsString();
  }

  std::unique_ptr<ChangeNotifier> change_notifier_;
};

}  // namespace

std::unique_ptr<BlobStorageChangeNotifier> BlobStorageChangeNotifier::Create(
    NotifierMetadata metadata) {
  return std::make_unique<S3BlobStorageChangeNotifier>(
      std::move(metadata.sns_arn));
}

}  // namespace kv_server
