const { app, BrowserWindow } = require('electron');

app.on('web-contents-created', () => {
  throw new Error();
});

function createWindow () {
  const mainWindow = new BrowserWindow({
    show: false
  });

  mainWindow.loadFile('index.html');
}

app.whenReady().then(() => {
  createWindow();

  app.quit();
});
