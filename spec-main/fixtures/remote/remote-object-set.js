const { BrowserWindow } = require('electron');

class Foo {
  // eslint-disable-next-line accessor-pairs
  set bar(value) {
    if (!(value instanceof BrowserWindow)) {
      throw new Error('setting error');
    }
  }
}

module.exports = new Foo();
