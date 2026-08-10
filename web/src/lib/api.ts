const BASE = '/api';

export async function apiGet<T>(path: string): Promise<T> {
  const res = await fetch(`${BASE}${path}`);
  if (!res.ok) {
    throw new Error(`GET ${path} failed: ${res.status}`);
  }
  return res.json();
}

export async function apiPost<T>(
  path: string,
  token: string,
  body?: unknown,
): Promise<T> {
  const res = await fetch(`${BASE}${path}`, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'X-Setup-Token': token,
    },
    body: body ? JSON.stringify(body) : undefined,
  });
  if (!res.ok) {
    const err = await res.json().catch(() => ({ error: res.statusText }));
    throw new Error(err.error ?? `POST ${path} failed`);
  }
  return res.json();
}

export interface DeviceInfo {
  eui64: string;
  firmware_version: string;
  chip_model: string;
  free_heap: number;
}

export interface Neighbor {
  rloc16: number;
  ext_mac: string;
  avg_rssi: number;
  is_child: boolean;
}
