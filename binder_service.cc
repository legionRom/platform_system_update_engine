//
// Copyright (C) 2015 The Android Open Source Project
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
//

#include "update_engine/binder_service.h"

#include <base/bind.h>

#include <binderwrapper/binder_wrapper.h>

#include <utils/String16.h>
#include <utils/StrongPointer.h>

using android::String16;
using android::String8;
using android::binder::Status;
using android::brillo::IUpdateEngineStatusCallback;
using android::brillo::ParcelableUpdateEngineStatus;
using android::sp;
using brillo::ErrorPtr;
using std::string;

namespace chromeos_update_engine {

namespace {
string NormalString(const String16& in) {
  return string{String8{in}.string()};
}

Status ToStatus(ErrorPtr* error) {
  return Status::fromServiceSpecificError(
      1, String8{error->get()->GetMessage().c_str()});
}
}  // namespace

template<typename... Parameters, typename... Arguments>
Status BinderUpdateEngineService::CallCommonHandler(
    bool (UpdateEngineService::*Handler)(ErrorPtr*, Parameters...),
    Arguments... arguments) {
  ErrorPtr error;
  if (((common_.get())->*Handler)(&error, arguments...)) return Status::ok();
  return ToStatus(&error);
}

Status BinderUpdateEngineService::AttemptUpdate(const String16& app_version,
                                                const String16& omaha_url,
                                                int flags) {
  return CallCommonHandler(
      &UpdateEngineService::AttemptUpdate, NormalString(app_version),
      NormalString(omaha_url), flags);
}

Status BinderUpdateEngineService::AttemptRollback(bool powerwash) {
  return CallCommonHandler(&UpdateEngineService::AttemptRollback, powerwash);
}

Status BinderUpdateEngineService::CanRollback(bool* out_can_rollback) {
  return CallCommonHandler(&UpdateEngineService::CanRollback,
                           out_can_rollback);
}

Status BinderUpdateEngineService::ResetStatus() {
  return CallCommonHandler(&UpdateEngineService::ResetStatus);
}

Status BinderUpdateEngineService::GetStatus(
    ParcelableUpdateEngineStatus* status) {
  string current_op;
  string new_version;

  auto ret = CallCommonHandler(&UpdateEngineService::GetStatus,
                      &status->last_checked_time_,
                      &status->progress_,
                      &current_op,
                      &new_version,
                      &status->new_size_);

  if (ret.isOk()) {
    status->current_operation_ = String16{current_op.c_str()};
    status->new_version_ = String16{new_version.c_str()};
  }

  return ret;
}

Status BinderUpdateEngineService::RebootIfNeeded() {
  return CallCommonHandler(&UpdateEngineService::RebootIfNeeded);
}

Status BinderUpdateEngineService::SetChannel(const String16& target_channel,
                                             bool powerwash) {
  return CallCommonHandler(&UpdateEngineService::SetChannel,
                           NormalString(target_channel), powerwash);
}

Status BinderUpdateEngineService::GetChannel(bool get_current_channel,
                                             String16* out_channel) {
  string channel_string;
  auto ret = CallCommonHandler(&UpdateEngineService::GetChannel,
                               get_current_channel,
                               &channel_string);

  *out_channel = String16(channel_string.c_str());
  return ret;
}

Status BinderUpdateEngineService::SetP2PUpdatePermission(bool enabled) {
  return CallCommonHandler(&UpdateEngineService::SetP2PUpdatePermission,
                           enabled);
}

Status BinderUpdateEngineService::GetP2PUpdatePermission(
    bool* out_p2p_permission) {
  return CallCommonHandler(&UpdateEngineService::GetP2PUpdatePermission,
                           out_p2p_permission);
}

Status BinderUpdateEngineService::SetUpdateOverCellularPermission(
    bool enabled) {
  return CallCommonHandler(
      &UpdateEngineService::SetUpdateOverCellularPermission, enabled);
}

Status BinderUpdateEngineService::GetUpdateOverCellularPermission(
    bool* out_cellular_permission) {
  return CallCommonHandler(
      &UpdateEngineService::GetUpdateOverCellularPermission,
      out_cellular_permission);
}

Status BinderUpdateEngineService::GetDurationSinceUpdate(
    int64_t* out_duration) {
  return CallCommonHandler(&UpdateEngineService::GetDurationSinceUpdate,
                           out_duration);
}

Status BinderUpdateEngineService::GetPrevVersion(String16* out_prev_version) {
  string version_string;
  auto ret = CallCommonHandler(&UpdateEngineService::GetPrevVersion,
                               &version_string);

  *out_prev_version = String16(version_string.c_str());
  return ret;
}

Status BinderUpdateEngineService::GetRollbackPartition(
    String16* out_rollback_partition) {
  string partition_string;
  auto ret = CallCommonHandler(&UpdateEngineService::GetRollbackPartition,
                               &partition_string);

  if (ret.isOk()) {
    *out_rollback_partition = String16(partition_string.c_str());
  }

  return ret;
}

Status BinderUpdateEngineService::RegisterStatusCallback(
    const sp<IUpdateEngineStatusCallback>& callback) {
  callbacks_.emplace_back(callback);

  auto binder_wrapper = android::BinderWrapper::Get();

  binder_wrapper->RegisterForDeathNotifications(
      IUpdateEngineStatusCallback::asBinder(callback),
      base::Bind(&BinderUpdateEngineService::UnregisterStatusCallback,
                 base::Unretained(this), base::Unretained(callback.get())));

  return Status::ok();
}

void BinderUpdateEngineService::UnregisterStatusCallback(
    IUpdateEngineStatusCallback* callback) {
  auto it = callbacks_.begin();

  for (; it != callbacks_.end() && it->get() != callback; it++)
    ;

  if (it == callbacks_.end()) {
    LOG(ERROR) << "Got death notification for unknown callback.";
    return;
  }

  LOG(INFO) << "Erasing orphan callback";
  callbacks_.erase(it);
}

void BinderUpdateEngineService::SendStatusUpdate(
    int64_t in_last_checked_time, double in_progress,
    const std::string& in_current_operation, const std::string& in_new_version,
    int64_t in_new_size) {
  for (auto& callback : callbacks_) {
    callback->HandleStatusUpdate(in_last_checked_time, in_progress,
                                 String16{in_current_operation.c_str()},
                                 String16{in_new_version.c_str()}, in_new_size);
  }
}

}  // namespace chromeos_update_engine
