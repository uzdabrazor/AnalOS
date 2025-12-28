/// <reference path="../../types/chrome-browser-os.d.ts" />

import { getFeatureFlags } from '@/lib/utils/featureFlags'

// ============= Re-export types from chrome.analOS namespace =============

export type InteractiveNode = chrome.analOS.InteractiveNode;
export type InteractiveSnapshot = chrome.analOS.InteractiveSnapshot;
export type InteractiveSnapshotOptions =
  chrome.analOS.InteractiveSnapshotOptions;
export type PageLoadStatus = chrome.analOS.PageLoadStatus;
export type InteractiveNodeType = chrome.analOS.InteractiveNodeType;
export type Rect = chrome.analOS.BoundingRect;

// Snapshot types (old format - sections-based)
export type SnapshotType = chrome.analOS.SnapshotType;
export type SnapshotContext = chrome.analOS.SnapshotContext;
export type SectionType = chrome.analOS.SectionType;
export type TextSnapshotResult = chrome.analOS.TextSnapshotResult;
export type LinkInfo = chrome.analOS.LinkInfo;
export type LinksSnapshotResult = chrome.analOS.LinksSnapshotResult;
export type SnapshotSection = chrome.analOS.SnapshotSection;
export type Snapshot = chrome.analOS.Snapshot;
export type SnapshotOptions = chrome.analOS.SnapshotOptions;

// Snapshot types (new format - items-based)
export type NewSnapshotItem = chrome.analOS.NewSnapshotItem;
export type NewSnapshot = chrome.analOS.NewSnapshot;

// Union type for both old and new snapshot formats
export type SnapshotResult = Snapshot | NewSnapshot;

// Preferences types
export type PrefObject = chrome.analOS.PrefObject;

// ============= AnalOS Adapter =============

// Screenshot size constants
export const SCREENSHOT_SIZES = {
  small: 512, // Low token usage
  medium: 768, // Balanced (default)
  large: 1028, // High detail (note: 1028 not 1024)
} as const;

export type ScreenshotSizeKey = keyof typeof SCREENSHOT_SIZES;

/**
 * Adapter for Chrome AnalOS Extension APIs
 * Provides a clean interface to analOS functionality with extensibility
 */
export class AnalOSAdapter {
  private static instance: AnalOSAdapter | null = null;

  private constructor() {}

  /**
   * Get singleton instance
   */
  static getInstance(): AnalOSAdapter {
    if (!AnalOSAdapter.instance) {
      AnalOSAdapter.instance = new AnalOSAdapter();
    }
    return AnalOSAdapter.instance;
  }

  /**
   * Get interactive snapshot of the current page
   */
  async getInteractiveSnapshot(
    tabId: number,
    options?: InteractiveSnapshotOptions,
  ): Promise<InteractiveSnapshot> {
    try {
      console.log(
        `[AnalOSAdapter] Getting interactive snapshot for tab ${tabId} with options: ${JSON.stringify(options)}`,
      );

      return new Promise<InteractiveSnapshot>((resolve, reject) => {
        if (options) {
          chrome.analOS.getInteractiveSnapshot(
            tabId,
            options,
            (snapshot: InteractiveSnapshot) => {
              if (chrome.runtime.lastError) {
                reject(new Error(chrome.runtime.lastError?.message || 'Unknown error'));
              } else {
                console.log(
                  `[AnalOSAdapter] Retrieved snapshot with ${snapshot.elements.length} elements`,
                );
                resolve(snapshot);
              }
            },
          );
        } else {
          chrome.analOS.getInteractiveSnapshot(
            tabId,
            (snapshot: InteractiveSnapshot) => {
              if (chrome.runtime.lastError) {
                reject(new Error(chrome.runtime.lastError?.message || 'Unknown error'));
              } else {
                console.log(
                  `[AnalOSAdapter] Retrieved snapshot with ${snapshot.elements.length} elements`,
                );
                resolve(snapshot);
              }
            },
          );
        }
      });
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      console.error(
        `[AnalOSAdapter] Failed to get interactive snapshot: ${errorMessage}`,
      );
      throw new Error(`Failed to get interactive snapshot: ${errorMessage}`);
    }
  }

  /**
   * Click an element by node ID
   */
  async click(tabId: number, nodeId: number): Promise<void> {
    try {
      console.log(`[AnalOSAdapter] Clicking node ${nodeId} in tab ${tabId}`);

      return new Promise<void>((resolve, reject) => {
        chrome.analOS.click(tabId, nodeId, () => {
          if (chrome.runtime.lastError) {
            reject(new Error(chrome.runtime.lastError?.message || 'Unknown error'));
          } else {
            resolve();
          }
        });
      });
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      console.error(`[AnalOSAdapter] Failed to click node: ${errorMessage}`);
      throw new Error(`Failed to click node ${nodeId}: ${errorMessage}`);
    }
  }

  /**
   * Input text into an element
   */
  async inputText(tabId: number, nodeId: number, text: string): Promise<void> {
    try {
      console.log(
        `[AnalOSAdapter] Inputting text into node ${nodeId} in tab ${tabId}`,
      );

      return new Promise<void>((resolve, reject) => {
        chrome.analOS.inputText(tabId, nodeId, text, () => {
          if (chrome.runtime.lastError) {
            reject(new Error(chrome.runtime.lastError?.message || 'Unknown error'));
          } else {
            resolve();
          }
        });
      });
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      console.error(`[AnalOSAdapter] Failed to input text: ${errorMessage}`);
      throw new Error(
        `Failed to input text into node ${nodeId}: ${errorMessage}`,
      );
    }
  }

  /**
   * Clear text from an element
   */
  async clear(tabId: number, nodeId: number): Promise<void> {
    try {
      console.log(`[AnalOSAdapter] Clearing node ${nodeId} in tab ${tabId}`);

      return new Promise<void>((resolve, reject) => {
        chrome.analOS.clear(tabId, nodeId, () => {
          if (chrome.runtime.lastError) {
            reject(new Error(chrome.runtime.lastError?.message || 'Unknown error'));
          } else {
            resolve();
          }
        });
      });
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      console.error(`[AnalOSAdapter] Failed to clear node: ${errorMessage}`);
      throw new Error(`Failed to clear node ${nodeId}: ${errorMessage}`);
    }
  }

  /**
   * Scroll to a specific node
   */
  async scrollToNode(tabId: number, nodeId: number): Promise<boolean> {
    try {
      console.log(
        `[AnalOSAdapter] Scrolling to node ${nodeId} in tab ${tabId}`,
      );

      return new Promise<boolean>((resolve, reject) => {
        chrome.analOS.scrollToNode(tabId, nodeId, (scrolled: boolean) => {
          if (chrome.runtime.lastError) {
            reject(new Error(chrome.runtime.lastError?.message || 'Unknown error'));
          } else {
            resolve(scrolled);
          }
        });
      });
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      console.error(
        `[AnalOSAdapter] Failed to scroll to node: ${errorMessage}`,
      );
      throw new Error(`Failed to scroll to node ${nodeId}: ${errorMessage}`);
    }
  }

  /**
   * Send keyboard keys
   */
  async sendKeys(tabId: number, keys: chrome.analOS.Key): Promise<void> {
    try {
      console.log(`[AnalOSAdapter] Sending keys "${keys}" to tab ${tabId}`);

      return new Promise<void>((resolve, reject) => {
        chrome.analOS.sendKeys(tabId, keys, () => {
          if (chrome.runtime.lastError) {
            reject(new Error(chrome.runtime.lastError?.message || 'Unknown error'));
          } else {
            resolve();
          }
        });
      });
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      console.error(`[AnalOSAdapter] Failed to send keys: ${errorMessage}`);
      throw new Error(`Failed to send keys: ${errorMessage}`);
    }
  }

  /**
   * Get page load status
   */
  async getPageLoadStatus(tabId: number): Promise<PageLoadStatus> {
    try {
      console.log(
        `[AnalOSAdapter] Getting page load status for tab ${tabId}`,
      );

      return new Promise<PageLoadStatus>((resolve, reject) => {
        chrome.analOS.getPageLoadStatus(tabId, (status: PageLoadStatus) => {
          if (chrome.runtime.lastError) {
            reject(new Error(chrome.runtime.lastError?.message || 'Unknown error'));
          } else {
            resolve(status);
          }
        });
      });
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      console.error(
        `[AnalOSAdapter] Failed to get page load status: ${errorMessage}`,
      );
      throw new Error(`Failed to get page load status: ${errorMessage}`);
    }
  }

  /**
   * Get accessibility tree (if available)
   */
  async getAccessibilityTree(
    tabId: number,
  ): Promise<chrome.analOS.AccessibilityTree> {
    try {
      console.log(
        `[AnalOSAdapter] Getting accessibility tree for tab ${tabId}`,
      );

      return new Promise<chrome.analOS.AccessibilityTree>(
        (resolve, reject) => {
          chrome.analOS.getAccessibilityTree(
            tabId,
            (tree: chrome.analOS.AccessibilityTree) => {
              if (chrome.runtime.lastError) {
                reject(new Error(chrome.runtime.lastError?.message || 'Unknown error'));
              } else {
                resolve(tree);
              }
            },
          );
        },
      );
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      console.error(
        `[AnalOSAdapter] Failed to get accessibility tree: ${errorMessage}`,
      );
      throw new Error(`Failed to get accessibility tree: ${errorMessage}`);
    }
  }

  /**
   * Capture a screenshot of the tab
   * @param tabId - The tab ID to capture
   * @param size - Optional screenshot size ('small', 'medium', or 'large')
   * @param showHighlights - Optional flag to show element highlights
   * @param width - Optional exact width for screenshot
   * @param height - Optional exact height for screenshot
   */
  async captureScreenshot(
    tabId: number,
    size?: ScreenshotSizeKey,
    showHighlights?: boolean,
    width?: number,
    height?: number,
  ): Promise<string> {
    try {
      const sizeDesc = size ? ` (${size})` : "";
      const highlightDesc = showHighlights ? " with highlights" : "";
      const dimensionsDesc = width && height ? ` (${width}x${height})` : "";
      console.log(
        `[AnalOSAdapter] Capturing screenshot for tab ${tabId}${sizeDesc}${highlightDesc}${dimensionsDesc}`,
      );

      return new Promise<string>((resolve, reject) => {
        // Use exact dimensions if provided
        if (width !== undefined && height !== undefined) {
          chrome.analOS.captureScreenshot(
            tabId,
            0, // thumbnailSize ignored when width/height specified
            showHighlights || false,
            width,
            height,
            (dataUrl: string) => {
              if (chrome.runtime.lastError) {
                reject(new Error(chrome.runtime.lastError?.message || 'Unknown error'));
              } else {
                console.log(
                  `[AnalOSAdapter] Screenshot captured for tab ${tabId} (${width}x${height})${highlightDesc}`,
                );
                resolve(dataUrl);
              }
            },
          );
        } else if (size !== undefined || showHighlights !== undefined) {
          const pixelSize = size ? SCREENSHOT_SIZES[size] : 0;
          // Use the API with thumbnail size and highlights
          if (showHighlights !== undefined) {
            chrome.analOS.captureScreenshot(
              tabId,
              pixelSize,
              showHighlights,
              (dataUrl: string) => {
                if (chrome.runtime.lastError) {
                  reject(new Error(chrome.runtime.lastError?.message || 'Unknown error'));
                } else {
                  console.log(
                    `[AnalOSAdapter] Screenshot captured for tab ${tabId}${sizeDesc}${highlightDesc}`,
                  );
                  resolve(dataUrl);
                }
              },
            );
          } else {
            chrome.analOS.captureScreenshot(
              tabId,
              pixelSize,
              (dataUrl: string) => {
                if (chrome.runtime.lastError) {
                  reject(new Error(chrome.runtime.lastError?.message || 'Unknown error'));
                } else {
                  console.log(
                    `[AnalOSAdapter] Screenshot captured for tab ${tabId} (${size}: ${pixelSize}px)`,
                  );
                  resolve(dataUrl);
                }
              },
            );
          }
        } else {
          // Use the original API without size (backwards compatibility)
          chrome.analOS.captureScreenshot(tabId, (dataUrl: string) => {
            if (chrome.runtime.lastError) {
              reject(new Error(chrome.runtime.lastError?.message || 'Unknown error'));
            } else {
              console.log(
                `[AnalOSAdapter] Screenshot captured for tab ${tabId}`,
              );
              resolve(dataUrl);
            }
          });
        }
      });
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      console.error(
        `[AnalOSAdapter] Failed to capture screenshot: ${errorMessage}`,
      );
      throw new Error(`Failed to capture screenshot: ${errorMessage}`);
    }
  }

  /**
   * Get a content snapshot of the specified type from the page
   * Returns old format (sections) or new format (items) based on feature flag
   * @param tabId - Tab ID to get snapshot from
   * @param type - Type of snapshot ('text' or 'links')
   * @param options - Optional snapshot options
   */
  async getSnapshot(
    tabId: number,
    type: SnapshotType,
    options?: SnapshotOptions,
  ): Promise<SnapshotResult> {
    try {
      // Check feature flag for new format
      const featureFlags = getFeatureFlags();
      const useNewFormat = featureFlags.isEnabled('NEW_SNAPSHOT_FORMAT');

      console.log(
        `[AnalOSAdapter] Getting snapshot for tab ${tabId} with type ${type}, newFormat: ${useNewFormat}, options: ${JSON.stringify(options)}`,
      );

      return new Promise<Snapshot>((resolve, reject) => {
        const callback = (snapshot: Snapshot) => {
          if (chrome.runtime.lastError) {
            reject(new Error(chrome.runtime.lastError.message));
          } else {
            console.log(
              `[AnalOSAdapter] Retrieved snapshot: ${JSON.stringify(snapshot)}`,
            );
            resolve(snapshot);
          }
        };

        // Old format: getSnapshot(tabId, type, [options], callback)
        // New format: getSnapshot(tabId, [options], callback)
        if (useNewFormat) {
          if (options) {
            chrome.analOS.getSnapshot(tabId, options, callback);
          } else {
            chrome.analOS.getSnapshot(tabId, callback);
          }
        } else {
          if (options) {
            chrome.analOS.getSnapshot(tabId, type, options, callback);
          } else {
            chrome.analOS.getSnapshot(tabId, type, callback);
          }
        }
      });
    } catch (error) {
      const errorMessage = error instanceof Error ? error.message : String(error);
      console.error(`[AnalOSAdapter] Failed to get snapshot: ${errorMessage}`);
      throw new Error(`Failed to get snapshot: ${errorMessage}`);
    }
  }

  /**
   * Get text content snapshot from the page
   * Convenience method for text snapshot
   * Returns old or new format based on feature flag
   */
  async getTextSnapshot(
    tabId: number,
    options?: SnapshotOptions,
  ): Promise<SnapshotResult> {
    return this.getSnapshot(tabId, "text", options);
  }

  /**
   * Get links snapshot from the page
   * Convenience method for links snapshot
   * Returns old or new format based on feature flag
   */
  async getLinksSnapshot(
    tabId: number,
    options?: SnapshotOptions,
  ): Promise<SnapshotResult> {
    return this.getSnapshot(tabId, "links", options);
  }

  /**
   * Generic method to invoke any AnalOS API
   * Useful for future APIs or experimental features
   */
  async invokeAPI(method: string, ...args: any[]): Promise<any> {
    try {
      console.log(`[AnalOSAdapter] Invoking AnalOS API: ${method}`);

      if (!(method in chrome.analOS)) {
        throw new Error(`Unknown AnalOS API method: ${method}`);
      }

      // @ts-expect-error - Dynamic API invocation
      const result = await chrome.analOS[method](...args);
      return result;
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      console.error(
        `[AnalOSAdapter] Failed to invoke API ${method}: ${errorMessage}`,
      );
      throw new Error(
        `Failed to invoke AnalOS API ${method}: ${errorMessage}`,
      );
    }
  }

  /**
   * Check if a specific API is available
   */
  isAPIAvailable(method: string): boolean {
    return method in chrome.analOS;
  }

  /**
   * Get list of available AnalOS APIs
   */
  getAvailableAPIs(): string[] {
    return Object.keys(chrome.analOS).filter((key) => {
      // @ts-expect-error - Dynamic key access for API discovery
      return typeof chrome.analOS[key] === "function";
    });
  }

  /**
   * Get AnalOS version information
   */
  async getVersion(): Promise<string | null> {
    try {
      console.log("[AnalOSAdapter] Getting AnalOS version");

      return new Promise<string | null>((resolve, reject) => {
        // Check if getVersionNumber API is available
        if (
          "getVersionNumber" in chrome.analOS &&
          typeof chrome.analOS.getVersionNumber === "function"
        ) {
          chrome.analOS.getVersionNumber((version: string) => {
            if (chrome.runtime.lastError) {
              reject(new Error(chrome.runtime.lastError?.message || 'Unknown error'));
            } else {
              console.log(`[AnalOSAdapter] AnalOS version: ${version}`);
              resolve(version);
            }
          });
        } else {
          // Fallback - return null if API not available
          resolve(null);
        }
      });
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      console.error(
        `[AnalOSAdapter] Failed to get version: ${errorMessage}`,
      );
      // Return null on error
      return null;
    }
  }

  /**
   * Log a metric event with optional properties
   */
  async logMetric(
    eventName: string,
    properties?: Record<string, any>,
  ): Promise<void> {
    try {
      console.log(
        `[AnalOSAdapter] Logging metric: ${eventName} with properties: ${JSON.stringify(properties)}`,
      );

      return new Promise<void>((resolve, reject) => {
        // Check if logMetric API is available
        if (
          "logMetric" in chrome.analOS &&
          typeof chrome.analOS.logMetric === "function"
        ) {
          if (properties) {
            chrome.analOS.logMetric(eventName, properties, () => {
              if (chrome.runtime.lastError) {
                reject(new Error(chrome.runtime.lastError?.message || 'Unknown error'));
              } else {
                console.log(`[AnalOSAdapter] Metric logged: ${eventName}`);
                resolve();
              }
            });
          } else {
            chrome.analOS.logMetric(eventName, () => {
              if (chrome.runtime.lastError) {
                reject(new Error(chrome.runtime.lastError?.message || 'Unknown error'));
              } else {
                console.log(`[AnalOSAdapter] Metric logged: ${eventName}`);
                resolve();
              }
            });
          }
        } else {
          // If API not available, log a warning but don't fail
          console.warn(
            `[AnalOSAdapter] logMetric API not available, skipping metric: ${eventName}`,
          );
          resolve();
        }
      });
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      console.error(`[AnalOSAdapter] Failed to log metric: ${errorMessage}`);
      return;
    }
  }

  /**
   * Execute JavaScript code in the specified tab
   * @param tabId - The tab ID to execute code in
   * @param code - The JavaScript code to execute
   * @returns The result of the execution
   */
  async executeJavaScript(tabId: number, code: string): Promise<any> {
    try {
      console.log(
        `[AnalOSAdapter] Executing JavaScript in tab ${tabId}`,
      );

      return new Promise<any>((resolve, reject) => {
        // Check if executeJavaScript API is available
        if (
          "executeJavaScript" in chrome.analOS &&
          typeof chrome.analOS.executeJavaScript === "function"
        ) {
          chrome.analOS.executeJavaScript(tabId, code, (result: any) => {
            if (chrome.runtime.lastError) {
              reject(new Error(chrome.runtime.lastError?.message || 'Unknown error'));
            } else {
              console.log(
                `[AnalOSAdapter] JavaScript executed successfully in tab ${tabId}`,
              );
              resolve(result);
            }
          });
        } else {
          reject(new Error("executeJavaScript API not available"));
        }
      });
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      console.error(
        `[AnalOSAdapter] Failed to execute JavaScript: ${errorMessage}`,
      );
      throw new Error(`Failed to execute JavaScript: ${errorMessage}`);
    }
  }

  /**
   * Click at specific viewport coordinates
   * @param tabId - The tab ID to click in
   * @param x - X coordinate in viewport pixels
   * @param y - Y coordinate in viewport pixels
   */
  async clickCoordinates(tabId: number, x: number, y: number): Promise<void> {
    try {
      console.log(
        `[AnalOSAdapter] Clicking at coordinates (${x}, ${y}) in tab ${tabId}`,
      );

      return new Promise<void>((resolve, reject) => {
        // Check if clickCoordinates API is available
        if (
          "clickCoordinates" in chrome.analOS &&
          typeof chrome.analOS.clickCoordinates === "function"
        ) {
          chrome.analOS.clickCoordinates(tabId, x, y, () => {
            if (chrome.runtime.lastError) {
              reject(new Error(chrome.runtime.lastError?.message || 'Unknown error'));
            } else {
              console.log(
                `[AnalOSAdapter] Successfully clicked at (${x}, ${y}) in tab ${tabId}`,
              );
              resolve();
            }
          });
        } else {
          reject(new Error("clickCoordinates API not available"));
        }
      });
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      console.error(
        `[AnalOSAdapter] Failed to click at coordinates: ${errorMessage}`,
      );
      throw new Error(`Failed to click at coordinates (${x}, ${y}): ${errorMessage}`);
    }
  }

  /**
   * Type text at specific viewport coordinates
   * @param tabId - The tab ID to type in
   * @param x - X coordinate in viewport pixels
   * @param y - Y coordinate in viewport pixels
   * @param text - Text to type at the location
   */
  async typeAtCoordinates(
    tabId: number,
    x: number,
    y: number,
    text: string,
  ): Promise<void> {
    try {
      console.log(
        `[AnalOSAdapter] Typing at coordinates (${x}, ${y}) in tab ${tabId}`,
      );

      return new Promise<void>((resolve, reject) => {
        // Check if typeAtCoordinates API is available
        if (
          "typeAtCoordinates" in chrome.analOS &&
          typeof chrome.analOS.typeAtCoordinates === "function"
        ) {
          chrome.analOS.typeAtCoordinates(tabId, x, y, text, () => {
            if (chrome.runtime.lastError) {
              reject(new Error(chrome.runtime.lastError?.message || 'Unknown error'));
            } else {
              console.log(
                `[AnalOSAdapter] Successfully typed "${text}" at (${x}, ${y}) in tab ${tabId}`,
              );
              resolve();
            }
          });
        } else {
          reject(new Error("typeAtCoordinates API not available"));
        }
      });
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      console.error(
        `[AnalOSAdapter] Failed to type at coordinates: ${errorMessage}`,
      );
      throw new Error(
        `Failed to type at coordinates (${x}, ${y}): ${errorMessage}`,
      );
    }
  }

  /**
   * Get a specific preference value
   * @param name - The preference name (e.g., "analos.server.mcp_port")
   * @returns Promise resolving to the preference object containing key, type, and value
   */
  async getPref(name: string): Promise<PrefObject> {
    try {
      console.log(`[AnalOSAdapter] Getting preference: ${name}`);

      // Check if chrome.analOS API is available
      if (!chrome?.analOS || typeof chrome.analOS.getPref !== 'function') {
        throw new Error('chrome.analOS.getPref is not available');
      }

      return new Promise<PrefObject>((resolve, reject) => {
        chrome.analOS.getPref(name, (pref: PrefObject) => {
          if (chrome.runtime.lastError) {
            reject(new Error(chrome.runtime.lastError?.message || 'Unknown error'));
          } else {
            console.log(
              `[AnalOSAdapter] Retrieved preference ${name}`,
            );
            resolve(pref);
          }
        });
      });
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      console.error(
        `[AnalOSAdapter] Failed to get preference: ${errorMessage}`,
      );
      throw new Error(`Failed to get preference ${name}: ${errorMessage}`);
    }
  }

  /**
   * Set a specific preference value
   * @param name - The preference name (e.g., "analos.server.mcp_enabled")
   * @param value - The value to set
   * @param pageId - Optional page ID for settings tracking
   * @returns Promise resolving to true if successful
   */
  async setPref(
    name: string,
    value: any,
    pageId?: string,
  ): Promise<boolean> {
    try {
      console.log(
        `[AnalOSAdapter] Setting preference ${name}`,
      );

      // Check if chrome.analOS API is available
      if (!chrome?.analOS || typeof chrome.analOS.setPref !== 'function') {
        throw new Error('chrome.analOS.setPref is not available');
      }

      return new Promise<boolean>((resolve, reject) => {
        if (pageId !== undefined) {
          chrome.analOS.setPref(name, value, pageId, (success: boolean) => {
            if (chrome.runtime.lastError) {
              reject(new Error(chrome.runtime.lastError?.message || 'Unknown error'));
            } else {
              console.log(
                `[AnalOSAdapter] Successfully set preference ${name}`,
              );
              resolve(success);
            }
          });
        } else {
          chrome.analOS.setPref(name, value, (success: boolean) => {
            if (chrome.runtime.lastError) {
              reject(new Error(chrome.runtime.lastError?.message || 'Unknown error'));
            } else {
              console.log(
                `[AnalOSAdapter] Successfully set preference ${name}`,
              );
              resolve(success);
            }
          });
        }
      });
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      console.error(
        `[AnalOSAdapter] Failed to set preference: ${errorMessage}`,
      );
      throw new Error(`Failed to set preference ${name}: ${errorMessage}`);
    }
  }

  /**
   * Get all preferences (filtered to analos.* prefs)
   * @returns Promise resolving to array of preference objects
   */
  async getAllPrefs(): Promise<PrefObject[]> {
    try {
      console.log("[AnalOSAdapter] Getting all preferences");

      // Check if chrome.analOS API is available
      if (!chrome?.analOS || typeof chrome.analOS.getAllPrefs !== 'function') {
        throw new Error('chrome.analOS.getAllPrefs is not available');
      }

      return new Promise<PrefObject[]>((resolve, reject) => {
        chrome.analOS.getAllPrefs((prefs: PrefObject[]) => {
          if (chrome.runtime.lastError) {
            reject(new Error(chrome.runtime.lastError?.message || 'Unknown error'));
          } else {
            console.log(
              `[AnalOSAdapter] Retrieved ${prefs.length} preferences`,
            );
            resolve(prefs);
          }
        });
      });
    } catch (error) {
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      console.error(
        `[AnalOSAdapter] Failed to get all preferences: ${errorMessage}`,
      );
      throw new Error(`Failed to get all preferences: ${errorMessage}`);
    }
  }
}

// Export singleton instance getter for convenience
export const getAnalOSAdapter = () => AnalOSAdapter.getInstance();
