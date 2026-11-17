const BASE = '/api';

export async function apiGet<T>(path: string): Promise<T> {
  const res = await fetch(`${BASE}${path}`);
  if (!res.ok) {
    throw new Error(`GET ${path} failed: ${res.status}`);
  }
  return res.json();
}

export async function apiGetAuth<T>(path: string, token: string): Promise<T> {
  const res = await fetch(`${BASE}${path}`, {
    headers: { 'X-Setup-Token': token },
  });
  if (!res.ok) {
    const err = await res.json().catch(() => ({ error: res.statusText }));
    throw new Error(err.error ?? `GET ${path} failed: ${res.status}`);
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

export interface SrpService {
  instance: string;
  service: string;
  port: number;
  matter?: 'operational' | 'commissionable' | 'commissioner';
  compressed_fabric_id?: string;
  node_id?: string;
}

export interface SrpHost {
  name: string;
  lease_remaining_s: number;
  addresses: string[];
  services: SrpService[];
  matter: boolean;
}

export interface SrpState {
  state: 'disabled' | 'running' | 'stopped' | 'unknown';
  auto_enable: boolean;
  address_mode: string;
  port: number;
  domain: string;
  matter_operational: number;
  matter_commissionable: number;
  hosts: SrpHost[];
}

export interface BorderRoutingState {
  state: string;
  omr_prefix: string | null;
  favored_omr_prefix: string | null;
  on_link_prefix: string | null;
  favored_on_link_prefix: string | null;
  nat64: {
    prefix_manager_state: string;
    local_prefix: string | null;
    favored_prefix: string | null;
  };
}
