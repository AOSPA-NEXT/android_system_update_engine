//
// Copyright (C) 2018 The Android Open Source Project
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

#ifndef UPDATE_ENGINE_DYNAMIC_PARTITION_CONTROL_ANDROID_H_
#define UPDATE_ENGINE_DYNAMIC_PARTITION_CONTROL_ANDROID_H_

#include <memory>
#include <set>
#include <string>

#include <base/files/file_util.h>
#include <libsnapshot/auto_device.h>
#include <libsnapshot/snapshot.h>

#include "update_engine/common/dynamic_partition_control_interface.h"

namespace chromeos_update_engine {

class DynamicPartitionControlAndroid : public DynamicPartitionControlInterface {
 public:
  DynamicPartitionControlAndroid();
  ~DynamicPartitionControlAndroid();
  FeatureFlag GetDynamicPartitionsFeatureFlag() override;
  FeatureFlag GetVirtualAbFeatureFlag() override;
  bool ShouldSkipOperation(const std::string& partition_name,
                           const InstallOperation& operation) override;
  void Cleanup() override;

  bool PreparePartitionsForUpdate(uint32_t source_slot,
                                  uint32_t target_slot,
                                  const DeltaArchiveManifest& manifest,
                                  bool update) override;
  bool FinishUpdate() override;

  // Return the device for partition |partition_name| at slot |slot|.
  // |current_slot| should be set to the current active slot.
  // Note: this function is only used by BootControl*::GetPartitionDevice.
  // Other callers should prefer BootControl*::GetPartitionDevice over
  // BootControl*::GetDynamicPartitionControl()->GetPartitionDevice().
  bool GetPartitionDevice(const std::string& partition_name,
                          uint32_t slot,
                          uint32_t current_slot,
                          std::string* device);

 protected:
  // These functions are exposed for testing.

  // Unmap logical partition on device mapper. This is the reverse operation
  // of MapPartitionOnDeviceMapper.
  // Returns true if unmapped successfully.
  virtual bool UnmapPartitionOnDeviceMapper(
      const std::string& target_partition_name);

  // Retrieve metadata from |super_device| at slot |source_slot|.
  //
  // If |target_slot| != kInvalidSlot, before returning the metadata, this
  // function modifies the metadata so that during updates, the metadata can be
  // written to |target_slot|. In particular, on retrofit devices, the returned
  // metadata automatically includes block devices at |target_slot|.
  //
  // If |target_slot| == kInvalidSlot, this function returns metadata at
  // |source_slot| without modifying it. This is the same as
  // LoadMetadataBuilder(const std::string&, uint32_t).
  virtual std::unique_ptr<android::fs_mgr::MetadataBuilder> LoadMetadataBuilder(
      const std::string& super_device,
      uint32_t source_slot,
      uint32_t target_slot);

  // Write metadata |builder| to |super_device| at slot |target_slot|.
  virtual bool StoreMetadata(const std::string& super_device,
                             android::fs_mgr::MetadataBuilder* builder,
                             uint32_t target_slot);

  // Map logical partition on device-mapper.
  // |super_device| is the device path of the physical partition ("super").
  // |target_partition_name| is the identifier used in metadata; for example,
  // "vendor_a"
  // |slot| is the selected slot to mount; for example, 0 for "_a".
  // Returns true if mapped successfully; if so, |path| is set to the device
  // path of the mapped logical partition.
  virtual bool MapPartitionOnDeviceMapper(
      const std::string& super_device,
      const std::string& target_partition_name,
      uint32_t slot,
      bool force_writable,
      std::string* path);

  // Return true if a static partition exists at device path |path|.
  virtual bool DeviceExists(const std::string& path);

  // Returns the current state of the underlying device mapper device
  // with given name.
  // One of INVALID, SUSPENDED or ACTIVE.
  virtual android::dm::DmDeviceState GetState(const std::string& name);

  // Returns the path to the device mapper device node in '/dev' corresponding
  // to 'name'. If the device does not exist, false is returned, and the path
  // parameter is not set.
  virtual bool GetDmDevicePathByName(const std::string& name,
                                     std::string* path);

  // Retrieve metadata from |super_device| at slot |source_slot|.
  virtual std::unique_ptr<android::fs_mgr::MetadataBuilder> LoadMetadataBuilder(
      const std::string& super_device, uint32_t source_slot);

  // Return a possible location for devices listed by name.
  virtual bool GetDeviceDir(std::string* path);

  // Return the name of the super partition (which stores super partition
  // metadata) for a given slot.
  virtual std::string GetSuperPartitionName(uint32_t slot);

  virtual void set_fake_mapped_devices(const std::set<std::string>& fake);

 private:
  friend class DynamicPartitionControlAndroidTest;

  void CleanupInternal();
  bool MapPartitionInternal(const std::string& super_device,
                            const std::string& target_partition_name,
                            uint32_t slot,
                            bool force_writable,
                            std::string* path);

  // Update |builder| according to |partition_metadata|, assuming the device
  // does not have Virtual A/B.
  bool UpdatePartitionMetadata(android::fs_mgr::MetadataBuilder* builder,
                               uint32_t target_slot,
                               const DeltaArchiveManifest& manifest);

  // Helper for PreparePartitionsForUpdate. Used for dynamic partitions without
  // Virtual A/B update.
  bool PrepareDynamicPartitionsForUpdate(uint32_t source_slot,
                                         uint32_t target_slot,
                                         const DeltaArchiveManifest& manifest);

  // Helper for PreparePartitionsForUpdate. Used for snapshotted partitions for
  // Virtual A/B update.
  bool PrepareSnapshotPartitionsForUpdate(uint32_t source_slot,
                                          uint32_t target_slot,
                                          const DeltaArchiveManifest& manifest);

  enum class DynamicPartitionDeviceStatus {
    SUCCESS,
    ERROR,
    TRY_STATIC,
  };

  // Return SUCCESS and path in |device| if partition is dynamic.
  // Return ERROR if any error.
  // Return TRY_STATIC if caller should resolve the partition as a static
  // partition instead.
  DynamicPartitionDeviceStatus GetDynamicPartitionDevice(
      const base::FilePath& device_dir,
      const std::string& partition_name_suffix,
      uint32_t slot,
      uint32_t current_slot,
      std::string* device);

  // Return true if |partition_name_suffix| is a block device of
  // super partition metadata slot |slot|.
  bool IsSuperBlockDevice(const base::FilePath& device_dir,
                          uint32_t current_slot,
                          const std::string& partition_name_suffix);

  std::set<std::string> mapped_devices_;
  const FeatureFlag dynamic_partitions_;
  const FeatureFlag virtual_ab_;
  std::unique_ptr<android::snapshot::SnapshotManager> snapshot_;
  std::unique_ptr<android::snapshot::AutoDevice> metadata_device_;
  bool target_supports_snapshot_ = false;
  // Whether the target partitions should be loaded as dynamic partitions. Set
  // by PreparePartitionsForUpdate() per each update.
  bool is_target_dynamic_ = false;
  uint32_t source_slot_ = UINT32_MAX;
  uint32_t target_slot_ = UINT32_MAX;

  DISALLOW_COPY_AND_ASSIGN(DynamicPartitionControlAndroid);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_DYNAMIC_PARTITION_CONTROL_ANDROID_H_
