const cp = require('child_process');

const cmd = process.argv[2];
const args = process.argv.slice(3);

const result = cp.spawnSync(`${cmd}.bat`, args, {
  stdio: 'inherit'
});

if (result.signal) {
  console.error('Subprocess died with signal:', result.signal);
  process.exit(1);
}

process.exit(result.status);
