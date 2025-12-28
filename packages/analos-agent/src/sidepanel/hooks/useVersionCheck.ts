import { useState, useEffect } from 'react'
import { getAnalOSAdapter } from '@/lib/browser/AnalOSAdapter'

const MIN_BROWSER_VERSION = 142

/**
 * Hook to check browser version and show upgrade warning for outdated browsers
 * Warning shows every session but can be dismissed for current session
 */
export function useVersionCheck() {
  const [currentVersion, setCurrentVersion] = useState<string | null>(null)
  const [showUpgradeWarning, setShowUpgradeWarning] = useState(false)

  useEffect(() => {
    const checkVersion = async () => {
      try {
        const adapter = getAnalOSAdapter()
        const version = await adapter.getVersion()
        setCurrentVersion(version)

        if (version) {
          // Extract major version: "137.0.7212.69" → 137
          const majorVersion = parseInt(version.split('.')[0], 10)
          setShowUpgradeWarning(majorVersion < MIN_BROWSER_VERSION)
        }
      } catch (error) {
        console.error('[useVersionCheck] Failed to get version:', error)
      }
    }

    checkVersion()
  }, [])

  // Dismiss for current session only (reappears on next open)
  const dismissWarning = () => {
    setShowUpgradeWarning(false)
  }

  return { showUpgradeWarning, currentVersion, dismissWarning }
}
