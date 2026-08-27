const TOKEN_STORAGE_KEY = 'ot_br_setup_token';

export function getStoredToken(): string {
  return localStorage.getItem(TOKEN_STORAGE_KEY) ?? '';
}

export function setStoredToken(token: string): void {
  localStorage.setItem(TOKEN_STORAGE_KEY, token);
  window.dispatchEvent(new CustomEvent('ot-br-token-changed'));
}

export function onTokenChanged(callback: () => void): () => void {
  window.addEventListener('ot-br-token-changed', callback);
  return () => window.removeEventListener('ot-br-token-changed', callback);
}
