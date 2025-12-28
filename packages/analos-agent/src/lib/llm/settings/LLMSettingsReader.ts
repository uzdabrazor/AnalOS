import { Logging } from '@/lib/utils/Logging'
import { isMockLLMSettings } from '@/config'
import {
  AnalOSProvider,
  AnalOSProvidersConfig,
  AnalOSProvidersConfigSchema,
  AnalOSPrefObject,
  ANALOS_PREFERENCE_KEYS,
  createDefaultAnalOSProvider,
  createDefaultProvidersConfig
} from './analOSTypes'

// Type definitions for chrome.analOS API (callback-based)
declare global {
  interface ChromeAnalOS {
    getPref(name: string, callback: (pref: AnalOSPrefObject) => void): void
    setPref(name: string, value: any, pageId?: string, callback?: (success: boolean) => void): void
    getAllPrefs(callback: (prefs: AnalOSPrefObject[]) => void): void
  }

  interface Chrome {
    analOS?: ChromeAnalOS
  }
}

const DEFAULT_OPENAI_MODEL = 'gpt-4o'
const DEFAULT_ANTHROPIC_MODEL = 'claude-3-5-sonnet-latest'
const DEFAULT_GEMINI_MODEL = 'gemini-2.0-flash'
const DEFAULT_OLLAMA_MODEL = 'qwen3:4b'
const DEFAULT_OLLAMA_BASE_URL = 'http://localhost:11434'

/**
 * Reads LLM provider settings from AnalOS preferences
 */
export class LLMSettingsReader {
  private static mockProvider: AnalOSProvider | null = null

  private static parseProvidersConfig(raw: unknown): AnalOSProvidersConfig | null {
    try {
      const data = typeof raw === 'string' ? JSON.parse(raw) : raw
      if (!data) return null
      const parsed = AnalOSProvidersConfigSchema.parse(data)
      return this.normalizeConfig(parsed)
    } catch (error) {
      Logging.log('LLMSettingsReader', `Failed to parse providers config: ${error}`, 'error')
      return null
    }
  }

  private static normalizeConfig(config: AnalOSProvidersConfig): AnalOSProvidersConfig {
    let defaultProviderId = config.defaultProviderId
    if (!config.providers.some(p => p.id === defaultProviderId)) {
      defaultProviderId = config.providers[0]?.id || 'analos'
    }

    const normalizedProviders = config.providers.map(provider => ({
      ...provider,
      isDefault: provider.id === defaultProviderId,
      isBuiltIn: provider.isBuiltIn ?? false,
      createdAt: provider.createdAt ?? new Date().toISOString(),
      updatedAt: provider.updatedAt ?? new Date().toISOString()
    }))

    if (normalizedProviders.length === 0) {
      return createDefaultProvidersConfig()
    }

    return {
      defaultProviderId,
      providers: normalizedProviders
    }
  }

  /**
   * Set mock provider for testing (DEV MODE ONLY)
   */
  static setMockProvider(provider: Partial<AnalOSProvider>): void {
    if (!isMockLLMSettings()) {
      Logging.log('LLMSettingsReader', 'setMockProvider is only available in development mode', 'warning')
      return
    }

    this.mockProvider = {
      ...this.getDefaultAnalOSProvider(),
      ...provider
    }
  }

  /**
   * Read the default provider configuration
   */
  static async read(): Promise<AnalOSProvider> {
    try {
      const config = await this.readAllProviders()
      const provider = config.providers.find(p => p.id === config.defaultProviderId)
        || config.providers[0]
        || this.getDefaultAnalOSProvider()
      return provider
    } catch (error) {
      const errorMessage = error instanceof Error ? error.message : String(error)
      Logging.log('LLMSettingsReader', `Failed to read settings: ${errorMessage}`, 'error')
      return this.getDefaultAnalOSProvider()
    }
  }

  /**
   * Read all providers configuration
   */
  static async readAllProviders(): Promise<AnalOSProvidersConfig> {
    try {
      const config = await this.readProvidersConfig()
      if (config) {
        return config
      }
    } catch (error) {
      const errorMessage = error instanceof Error ? error.message : String(error)
      Logging.log('LLMSettingsReader', `Failed to read providers: ${errorMessage}`, 'error')
    }

    return createDefaultProvidersConfig()
  }

  /**
   * Merge two provider configs, deduplicating by provider.id
   * Prefers providers with newer updatedAt timestamp
   */
  private static mergeProviderConfigs(
    config1: AnalOSProvidersConfig | null,
    config2: AnalOSProvidersConfig | null
  ): AnalOSProvidersConfig | null {
    if (!config1 && !config2) return null
    if (!config1) return config2
    if (!config2) return config1

    // Merge providers by id, preferring newer updatedAt
    const providerMap = new Map<string, AnalOSProvider>()

    for (const provider of config1.providers) {
      providerMap.set(provider.id, provider)
    }

    for (const provider of config2.providers) {
      const existing = providerMap.get(provider.id)
      if (!existing) {
        providerMap.set(provider.id, provider)
      } else {
        // Prefer provider with newer updatedAt timestamp
        const existingTime = new Date(existing.updatedAt || 0).getTime()
        const newTime = new Date(provider.updatedAt || 0).getTime()
        if (newTime > existingTime) {
          providerMap.set(provider.id, provider)
        }
      }
    }

    const mergedProviders = Array.from(providerMap.values())

    const config1Ids = new Set(config1.providers.map(p => p.id))
    const config2Ids = new Set(config2.providers.map(p => p.id))

    const candidateOrder: string[] = []

    const addCandidate = (id: string | undefined) => {
      if (!id) return
      if (!candidateOrder.includes(id)) {
        candidateOrder.push(id)
      }
    }

    const config1DefaultInBoth = config1Ids.has(config1.defaultProviderId)
      && config2Ids.has(config1.defaultProviderId)
    const config2DefaultInBoth = config2Ids.has(config2.defaultProviderId)
      && config1Ids.has(config2.defaultProviderId)

    if (config1DefaultInBoth) {
      addCandidate(config1.defaultProviderId)
    }

    if (config2DefaultInBoth) {
      addCandidate(config2.defaultProviderId)
    }

    addCandidate(config1.defaultProviderId)
    addCandidate(config2.defaultProviderId)

    const finalDefaultId = candidateOrder.find(id => mergedProviders.some(p => p.id === id))
      || mergedProviders[0]?.id
      || 'analos'

    return {
      defaultProviderId: finalDefaultId,
      providers: mergedProviders
    }
  }

  /**
   * Read full providers configuration with MERGE strategy
   * Reads from BOTH storages and merges to recover from stale data overwrites
   */
  private static async readProvidersConfig(): Promise<AnalOSProvidersConfig | null> {
    try {
      const key = ANALOS_PREFERENCE_KEYS.PROVIDERS
      let analOSConfig: AnalOSProvidersConfig | null = null
      let storageLocalConfig: AnalOSProvidersConfig | null = null

      // Read from AnalOS prefs
      if ((chrome as any)?.analOS?.getPref) {
        try {
          const pref = await new Promise<AnalOSPrefObject>((resolve, reject) => {
            (chrome as any).analOS.getPref(key, (pref: AnalOSPrefObject) => {
              if (chrome.runtime.lastError) {
                reject(chrome.runtime.lastError)
              } else {
                resolve(pref)
              }
            })
          })

          if (pref?.value) {
            const data = typeof pref.value === 'string' ? JSON.parse(pref.value) : pref.value
            // Ensure all providers have isDefault field
            if (data.providers) {
              data.providers = data.providers.map((p: any) => ({
                ...p,
                isDefault: p.isDefault !== undefined ? p.isDefault : (p.id === 'analos')
              }))
            }
            analOSConfig = AnalOSProvidersConfigSchema.parse(data)
          }
        } catch (getPrefError) {
          // Silently continue to fallback
        }
      }

      // Read from chrome.storage.local
      if (chrome.storage?.local) {
        const stored = await new Promise<any>((resolve) => {
          chrome.storage.local.get(key, (result) => resolve(result))
        })
        const raw = stored?.[key]
        if (raw) {
          const data = typeof raw === 'string' ? JSON.parse(raw) : raw
          // Ensure all providers have isDefault field
          if (data.providers) {
            data.providers = data.providers.map((p: any) => ({
              ...p,
              isDefault: p.isDefault !== undefined ? p.isDefault : (p.id === 'analos')
            }))
          }
          storageLocalConfig = AnalOSProvidersConfigSchema.parse(data)
        }
      }

      // Merge both configs
      const mergedConfig = this.mergeProviderConfigs(analOSConfig, storageLocalConfig)

      if (!mergedConfig) {
        return null
      }

      // Check if merge recovered providers - auto-save if recovery happened
      const analOSCount = analOSConfig?.providers.length || 0
      const storageLocalCount = storageLocalConfig?.providers.length || 0
      const mergedCount = mergedConfig.providers.length

      if (mergedCount > analOSCount || mergedCount > storageLocalCount) {
        await this.saveProvidersConfig(mergedConfig)
      }

      return mergedConfig
    } catch (error) {
      const errorMessage = error instanceof Error ? error.message : JSON.stringify(error)
      Logging.log('LLMSettingsReader', `Error reading providers: ${errorMessage}`, 'error')
      if (error instanceof Error && error.stack) {
        Logging.log('LLMSettingsReader', `Stack trace: ${error.stack}`, 'error')
      }
      return null
    }
  }

  static async saveProvidersConfig(config: AnalOSProvidersConfig): Promise<boolean> {
    const normalized = this.normalizeConfig(config)
    const key = ANALOS_PREFERENCE_KEYS.PROVIDERS
    const payload = JSON.stringify(normalized)

    let analOSSuccess = false
    let storageSuccess = false

    // Save to chrome.analOS.setPref (for AnalOS browser)
    if ((chrome as any)?.analOS?.setPref) {
      analOSSuccess = await new Promise<boolean>((resolve) => {
        (chrome as any).analOS.setPref(key, payload, undefined, (success?: boolean) => {
          const error = chrome.runtime?.lastError
          if (error) {
            Logging.log('LLMSettingsReader', `AnalOS setPref error: ${error.message}`, 'warning')
            resolve(false)
          } else if (success !== false) {
            resolve(true)
          } else {
            Logging.log('LLMSettingsReader', 'AnalOS setPref returned false', 'warning')
            resolve(false)
          }
        })
      })
    }

    // ALSO save to chrome.storage.local (always, for extension reliability)
    if (chrome.storage?.local) {
      storageSuccess = await new Promise((resolve) => {
        chrome.storage.local.set({ [key]: payload }, () => {
          if (chrome.runtime.lastError) {
            Logging.log('LLMSettingsReader', `chrome.storage.local save error: ${chrome.runtime.lastError.message}`, 'error')
            resolve(false)
          } else {
            resolve(true)
          }
        })
      })
    }

    // Success if either storage mechanism worked
    const success = analOSSuccess || storageSuccess
    if (!success) {
      Logging.log('LLMSettingsReader', 'Failed to save to any storage mechanism', 'error')
    }
    return success
  }

  private static getDefaultAnalOSProvider(): AnalOSProvider {
    return createDefaultAnalOSProvider()
  }

  private static getMockProvider(): AnalOSProvider {
    if (this.mockProvider) {
      return this.mockProvider
    }

    const mockType = process.env.MOCK_PROVIDER_TYPE || 'analos'

    const mockProviders: Record<string, AnalOSProvider> = {
      analos: this.getDefaultAnalOSProvider(),
      openai: {
        id: 'mock_openai',
        name: 'Mock OpenAI',
        type: 'openai',
        isDefault: true,
        isBuiltIn: false,
        baseUrl: 'https://api.openai.com/v1',
        apiKey: process.env.OPENAI_API_KEY || 'mock-key',
        modelId: DEFAULT_OPENAI_MODEL,
        capabilities: { supportsImages: true },
        modelConfig: { contextWindow: 128000, temperature: 0.7 },
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString()
      },
      anthropic: {
        id: 'mock_anthropic',
        name: 'Mock Anthropic',
        type: 'anthropic',
        isDefault: true,
        isBuiltIn: false,
        baseUrl: 'https://api.anthropic.com',
        apiKey: process.env.ANTHROPIC_API_KEY || 'mock-key',
        modelId: DEFAULT_ANTHROPIC_MODEL,
        capabilities: { supportsImages: true },
        modelConfig: { contextWindow: 200000, temperature: 0.7 },
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString()
      },
      gemini: {
        id: 'mock_gemini',
        name: 'Mock Gemini',
        type: 'google_gemini',
        isDefault: true,
        isBuiltIn: false,
        apiKey: process.env.GOOGLE_API_KEY || 'mock-key',
        modelId: DEFAULT_GEMINI_MODEL,
        capabilities: { supportsImages: true },
        modelConfig: { contextWindow: 1000000, temperature: 0.7 },
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString()
      },
      ollama: {
        id: 'mock_ollama',
        name: 'Mock Ollama',
        type: 'ollama',
        isDefault: true,
        isBuiltIn: false,
        baseUrl: DEFAULT_OLLAMA_BASE_URL,
        modelId: DEFAULT_OLLAMA_MODEL,
        capabilities: { supportsImages: false },
        modelConfig: { contextWindow: 4096, temperature: 0.7 },
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString()
      }
    }

    return mockProviders[mockType] || this.getDefaultAnalOSProvider()
  }
}







