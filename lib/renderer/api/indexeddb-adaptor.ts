
export class IndexeddbAdaptor extends BroadcastChannel  {
  db: any;
  store: any;
  __dbName: string;
  __storeName: string;
  __requireReopen: Boolean;

	constructor(dbName: string, storeName: string, broadcastChannelName: string) {
    super(broadcastChannelName);

    this.__dbName = dbName;
		this.__storeName = storeName;
		this.__requireReopen = false;
	}

  open() {
    return new Promise<any>((resolve, reject) => {
      this.__opendb('').then(result => {
        const { db, storeName }: any = result;
        return this.__transaction(db, storeName);
      }).then(result => {
        this.store = result;
        resolve(result);
      }).catch(error => {
        console.error(`cfg: indexeddb failed to open: ${error.message || error}`);
        this.__requireReopen = true;
        reject(error);
      });
    });
  }

  add(item: any): any {
    return new Promise<any>((resolve, reject) => {
      this.open().then((store: any) => {
        const request = store.add(item);
        request.addEventListener('success', (event: any) => {
          this.postMessage({ type: 'addUpdated', data: item });
          resolve(event.target.result);
        });
        request.addEventListener('error', (event: any) => {
          reject(event.target.error);
        });
      }).catch(err => {
        reject(err);
      });
    });
  }

  put(id: any, item: any) {
    return new Promise((resolve, reject) => {
      this.open().then((store: any) => {
        item.id = id;
        const request = store.put(item);
        request.addEventListener('success', (event: any) => {
          this.postMessage({ type: 'putUpdated', data: item });
          resolve(event.target.result);
        });
        request.addEventListener('error', (event: any) => {
          reject(event.target.error);
        });
      }).catch(err => {
        reject(err);
      });
    });
  }

  get(key: any) {
    return new Promise<any>((resolve, reject) => {
      this.open().then((store) => {
        const index = store.index("key");
        const request = index.get(key);
        request.addEventListener('success', (event: any) => {
          this.postMessage({ type: 'getUpdated', data: { key }});
          resolve(event.target.result);
        });
        request.addEventListener('error', (event: any) => {
          reject(event.target.error);
        });
      }).catch(err => {
        reject(err);
      });
    });
  }

  delete(key: any) {
    return new Promise<any>((resolve, reject) => {
      this.open().then((store) => {
        const request = store.delete(key);
        request.addEventListener('success', (event: any) => {
          this.postMessage({ type: 'deleteUpdated', data: { key }});
          resolve(event.target.result);
        });
        request.addEventListener('error', (event: any) => {
          reject(event.target.error);
        });
      }).catch(err => {
        reject(err);
      });
    });
  }

  getStoreKeys(index: any) {
    return new Promise<any>((resolve, reject) => {
      const keys: any = [];
      index = index || 'key';
      this.open().then((store) => {
        const tmpIndex = store.index(index);
        const cursorRequest = tmpIndex.openKeyCursor();
        cursorRequest.onsuccess = (event: any) => {
          const cursor = event.target.result;
          if (cursor) {
            keys.push(cursor.key);
            cursor.continue();
          } else {
            resolve(keys);
          }
        }
        cursorRequest.onerror = (event: any) => {
          reject(new Error(`failed to get store keys`));
        }
      }).catch(err => {
        reject(err);
      });
    });
  }

  getAll() {
    return new Promise<any>((resolve, reject) => {
      this.open().then((store) => {
        const ret: any = [];
        const request = store.openCursor();

        request.onsuccess = (event: any) => {
          var cursor = event.target.result;

          if (cursor) {
            const { value: item } = cursor;

            ret.push(item.value);
            cursor.continue();
            return;
          }

          resolve(ret);
        };

        request.onerror = (event: any) => {
          reject(event.target.error);
        };
      }).catch(err => {
        reject(err);
      });
    });
  }

  __opendb(version: any) {
    return new Promise<any>((resolve, reject) => {
      const [dbName, storeName] = [this.__dbName, this.__storeName];

      // try to reopen indexDB since failed to open indexDB last time
      if (this.db && !this.__requireReopen) {
        resolve({
          storeName,
          db: this.db
        });
        return;
      }
      this.__requireReopen = false;

      let request = null;
      if (version) {
        request = window.indexedDB.open(dbName, version);
      } else {
        request = window.indexedDB.open(dbName);
      }

      request.addEventListener('success', (event: any) => {
        const db = this.db = event.target.result

        if (!db.objectStoreNames.contains(storeName)) {
          reject({
            error: 'store is not existed',
            store: storeName,
            version: db.version + 1
          });
        } else {
          resolve({
            storeName: storeName,
            db: db
          });
        }
      });

      request.addEventListener('error', (event: any) => {
        reject(event.target.error);
      });

      request.addEventListener('upgradeneeded', (event: any) => {
        const db = event.target.result;

        if (!db.objectStoreNames.contains(storeName)) {
          const objectStore = db.createObjectStore(storeName, {
            keyPath: 'id',
            autoIncrement: 'true'
          });

          objectStore.createIndex('key', 'key', {
            unique: true
          });

          let trans = event.target.transaction;
          trans.oncomplete = () => {
            console.log(`Object Store: ${storeName} is created completed.`);
            this.db = db;
          }
        } else {
          this.db = db;
        }
      });
    });
  }

  __transaction(db: any, storeName: any) {
    return new Promise((resolve, reject) => {
      const transaction = db.transaction([storeName], 'readwrite');
      const store = transaction.objectStore(storeName);
      resolve(store);
    });
  }
}


export default IndexeddbAdaptor;
