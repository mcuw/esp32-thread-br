import qwik from '@qwik.dev/astro';
import { defineConfig } from 'astro/config';
import { loadEnv } from 'vite';

const { BOARD_IP } = loadEnv(
  process.env.NODE_ENV ?? 'development',
  process.cwd(),
  '',
);

// https://astro.build/config
export default defineConfig({
  output: 'static',
  integrations: [qwik()],
  vite: {
    server: {
      proxy: BOARD_IP
        ? {
            '/api': { target: `http://${BOARD_IP}`, changeOrigin: true },
          }
        : undefined,
    },
  },
});
