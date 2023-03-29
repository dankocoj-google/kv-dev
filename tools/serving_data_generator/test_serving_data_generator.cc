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

#include <fstream>
#include <iostream>
#include <string_view>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/substitute.h"
#include "glog/logging.h"
#include "google/protobuf/text_format.h"
#include "public/data_loading/data_loading_generated.h"
#include "public/data_loading/filename_utils.h"
#include "public/data_loading/records_utils.h"
#include "public/data_loading/riegeli_metadata.pb.h"
#include "riegeli/bytes/ostream_writer.h"
#include "riegeli/records/record_writer.h"

ABSL_FLAG(std::string, key, "foo", "Specify the key for lookups");
ABSL_FLAG(int, value_size, 100, "Specify the size of value for the key");
ABSL_FLAG(std::string, output_dir, "", "Output file directory");
ABSL_FLAG(int, num_records, 5, "Number of records to generate");
ABSL_FLAG(int64_t, timestamp, 123123123, "Record timestamp");

using kv_server::DeltaFileRecordStruct;
using kv_server::DeltaMutationType;
using kv_server::KVFileMetadata;
using kv_server::ToDeltaFileName;
using kv_server::ToStringView;

void WriteRecords(std::string_view key, int value_size,
                  riegeli::RecordWriterBase& writer) {
  const int repetition = absl::GetFlag(FLAGS_num_records);
  int64_t timestamp = absl::GetFlag(FLAGS_timestamp);

  for (int i = 0; i < repetition; ++i) {
    const std::string value(value_size, 'A' + (i % 50));
    writer.WriteRecord(ToStringView(DeltaFileRecordStruct{
        DeltaMutationType::Update, timestamp++, absl::StrCat(key, i), value}
                                        .ToFlatBuffer()));
  }
  LOG(INFO) << "write done";
}

int main(int argc, char** argv) {
  const std::vector<char*> commands = absl::ParseCommandLine(argc, argv);
  const std::string output_dir = absl::GetFlag(FLAGS_output_dir);

  auto write_records = [](std::ostream* os) {
    const std::string key = absl::GetFlag(FLAGS_key);
    const int value_size = absl::GetFlag(FLAGS_value_size);

    auto os_writer = riegeli::OStreamWriter(os);
    riegeli::RecordWriterBase::Options options;
    options.set_uncompressed();
    riegeli::RecordsMetadata metadata;
    KVFileMetadata file_metadata;

    *metadata.MutableExtension(kv_server::kv_file_metadata) = file_metadata;
    options.set_metadata(std::move(metadata));
    auto record_writer = riegeli::RecordWriter(std::move(os_writer), options);
    WriteRecords(key, value_size, record_writer);
    record_writer.Close();
  };

  if (output_dir == "-") {
    LOG(INFO) << "Writing records to console";

    write_records(&std::cout);
  } else {
    absl::Time now = absl::Now();
    if (const auto maybe_name = ToDeltaFileName(absl::ToUnixMicros(now));
        !maybe_name.ok()) {
      LOG(ERROR) << "Unable to construct file name: " << maybe_name.status();
      return -1;
    } else {
      const std::string outfile =
          absl::StrCat(output_dir, "/", maybe_name.value());
      LOG(INFO) << "Writing records to " << outfile;

      std::ofstream ofs(outfile);
      write_records(&ofs);
      ofs.close();
    }
  }
  return 0;
}
