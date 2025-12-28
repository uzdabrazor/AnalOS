import { z } from 'zod'

/**
 * AnalOS Provider type enum
 */
export const AnalOSProviderTypeSchema = z.enum([
  'analos',
  'openai',
  'openai_compatible',
  'anthropic',
  'google_gemini',
  'ollama',
  'openrouter',
  'custom'
])
export type AnalOSProviderType = z.infer<typeof AnalOSProviderTypeSchema>

/**
 * Provider capabilities configuration
 */
export const ProviderCapabilitiesSchema = z.object({
  supportsImages: z.boolean().optional()  // Whether the provider supports image inputs
})

/**
 * Model configuration for a provider
 */
export const ModelConfigSchema = z.object({
  contextWindow: z.union([z.number(), z.string()]).transform(val => {
    // Convert string to number if needed (from Chrome settings UI)
    return typeof val === 'string' ? parseInt(val, 10) : val
  }).optional(),  // Maximum context window size
  temperature: z.union([z.number(), z.string()]).transform(val => {
    // Convert string to number if needed (from Chrome settings UI)
    return typeof val === 'string' ? parseFloat(val) : val
  }).pipe(z.number().min(0).max(2)).optional()  // Default temperature setting
})

/**
 * Individual provider configuration from AnalOS
 */
export const AnalOSProviderSchema = z.object({
  id: z.string(),  // Unique provider identifier
  name: z.string(),  // Display name for the provider
  type: AnalOSProviderTypeSchema,  // Provider type
  isDefault: z.boolean(),  // Whether this is the default provider
  isBuiltIn: z.boolean(),  // Whether this is a built-in provider
  baseUrl: z.string().optional(),  // API base URL
  apiKey: z.string().optional(),  // API key for authentication
  modelId: z.string().optional(),  // Model identifier
  capabilities: ProviderCapabilitiesSchema.optional(),  // Provider capabilities
  modelConfig: ModelConfigSchema.optional(),  // Model configuration
  createdAt: z.string(),  // ISO timestamp of creation
  updatedAt: z.string()  // ISO timestamp of last update
})

export type AnalOSProvider = z.infer<typeof AnalOSProviderSchema>

/**
 * Complete AnalOS providers configuration
 */
export const AnalOSProvidersConfigSchema = z.object({
  defaultProviderId: z.string(),  // ID of the default provider
  providers: z.array(AnalOSProviderSchema)  // List of all providers
})

export type AnalOSProvidersConfig = z.infer<typeof AnalOSProvidersConfigSchema>

/**
 * Preference object returned by chrome.analOS.getPref
 */
export const AnalOSPrefObjectSchema = z.object({
  key: z.string(),  // Preference key
  type: z.string(),  // Preference type
  value: z.any()  // Preference value (string for JSON preferences)
})

export type AnalOSPrefObject = z.infer<typeof AnalOSPrefObjectSchema>

/**
 * Browser preference keys for AnalOS
 */
export const ANALOS_PREFERENCE_KEYS = {
  PROVIDERS: 'analos.providers'
} as const

export const DEFAULT_ANALOS_PROVIDER_ID = 'analos'

export function createDefaultAnalOSProvider(): AnalOSProvider {
  const timestamp = new Date().toISOString()
  return {
    id: DEFAULT_ANALOS_PROVIDER_ID,
    name: 'AnalOS',
    type: 'analos',
    isDefault: true,
    isBuiltIn: true,
    createdAt: timestamp,
    updatedAt: timestamp
  }
}

export function createDefaultProvidersConfig(): AnalOSProvidersConfig {
  const provider = createDefaultAnalOSProvider()
  return {
    defaultProviderId: provider.id,
    providers: [provider]
  }
}



