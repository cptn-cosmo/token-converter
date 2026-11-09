// SPDX-License-Identifier: GPL-3.0-or-later
// Human readable names for settings keys. Not a ".pragma library" so that the
// i18n functions of the importing component's context are available.

function label(key) {
    switch (key) {
    case "rules": return i18nc("@label setting", "Conversion rules");
    case "lossless": return i18nc("@label setting", "Lossless");
    case "quality": return i18nc("@label setting", "Quality");
    case "originalMode": return i18nc("@label setting", "Original files");
    case "dryRun": return i18nc("@label setting", "Preview before converting");
    case "skipIfLarger": return i18nc("@label setting", "Skip if larger");
    case "includePatterns": return i18nc("@label setting", "Include patterns");
    case "excludePatterns": return i18nc("@label setting", "Exclude patterns");
    case "excludeFolders": return i18nc("@label setting", "Excluded folders");
    case "prefix": return i18nc("@label setting", "Prefix");
    case "postfix": return i18nc("@label setting", "Postfix");
    case "filenameTemplate": return i18nc("@label setting", "Filename template");
    case "collisionMode": return i18nc("@label setting", "Name collisions");
    case "backupFolder": return i18nc("@label setting", "Backup folder");
    case "workerCount": return i18nc("@label setting", "Parallel workers");
    case "preserveMetadata": return i18nc("@label setting", "Preserve metadata");
    case "followSymlinks": return i18nc("@label setting", "Follow symlinks");
    }
    return key;
}
