const { app, BrowserWindow } = require('electron');

// function createWindow () {
//   const mainWindow = new BrowserWindow({
//     show: false
//   });

//   mainWindow.loadFile('index.html');
// }

// app.whenReady().then(() => {
//   createWindow();

//   app.quit();
//   process.exit(0);
// });

app.whenReady().then(() => {
  const mainWindow = new BrowserWindow({
    show: false
  });
  mainWindow.loadFile('index.html');

  app.on('web-contents-created', () => {
    throw new Error();
  });

  app.quit();
  // process.exit(0);
});

// app.whenReady().then(async () => {
//   const mainWindow = new BrowserWindow({ show: false });
//   await mainWindow.loadURL('about:blank');
//   process.exit(0);
// });
