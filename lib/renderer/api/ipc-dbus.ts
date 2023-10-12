import { EventEmitter } from 'events';

export class IpcDBus extends EventEmitter {
  service: string;
  objName: string;
  ifaceName: string;
  systemBus: any;
  serviceBus: any;
  iface: any;

  constructor (dbus: any, service: string, objName: string, ifaceName: string) {
    super();

    let systemBus = dbus.systemBus();
    this.serviceBus = systemBus.getService(service);
    this.service = service;
    this.objName = objName;
    this.ifaceName = ifaceName;

    // Do not throw exception when channel name is "error".
    // this.on('error', () => {});
  };

  async Service(retries = 1, interval: number): Promise<any> {
    if (this.iface) {
      return this.iface;
    }

    let delaySeconds = (sec = 1) => {
      return new Promise<Boolean>(resolve => {
          setTimeout(() => {
              resolve(true);
          }, sec * 1000);
      })
    }

    do {
      let iface = await new Promise<any>((resolve, reject) => {
        this.serviceBus.getInterface(
          this.objName, this.ifaceName, (err: any, iface: any) => {
          if (err) {
              reject(err);
              return;
          }
          this.iface = iface;
          resolve(iface);
        });
      }).catch(e => {
        if (retries <= 0) {
          throw e;
        }
      });

      if (iface) {
          return iface;
      } else {
        if (retries > 0) {
          if (interval < 0) {
            this.emit('error', 'dbus: value of interval is too small');
          } else {
            this.emit('error', "dbus: try to retry connect in "+ interval + "seconds");
            await delaySeconds(interval || 1);
          }
        }
      }
    } while (--retries > 0)
  };

  async ReadProp(property: any, retries: any, interval: any) {
    let iface =  await this.Service(retries, interval).catch(e => {
      throw e;
    });

    if (!iface) {
      return;
    }

    try {
      const result = await new Promise<any>((resolve, reject) => {
        iface.$readProp(property, (...args: any) => {
          if (args[0] && args[1] == undefined) {
            reject(args[0]);
          } else {
            resolve({err: args[0], ret: args[1]});
          }
        });
      });
      return result;
    } catch (e) {
      throw e;
    }
  }

  async WriteProp(property: any, retries: any, interval: any, ...args: any) {
    let iface =  await this.Service(retries, interval).catch(e => {
      throw e;
    });
    if (!iface) {
      return;
    }

    try {
      iface.$writeProp(property, ...args);
    } catch (e) {
      throw e;
    }
  }

  async Signal(signal: any, retries: any, interval: any, callback: any) {
    if (!callback) {
      throw Error('DBus signal callback is null!');
    }

    let iface =  await this.Service(retries, interval).catch(e => {
      throw e;
    });
    if (!iface)
      throw Error("Dbus connection is failed");

    try {
      iface.on(signal, (...args: any) => {
          callback(...args);
      });
    } catch (e) {
      throw e;
    }
  }

  async MethodCall(method: any, retries: any, interval: any, ...args: any) {
    let iface =  await this.Service(retries, interval).catch(e => {
      throw e;
    });
    if (!iface) {
      return;
    }

    return await new Promise<any>((resolve, reject) => {
      const handler = function (...args: any) {
        if (args[0] && args[1] == undefined) {
          reject(args[0]);
        } else {
          resolve({
            err: args[0], ret: args[1], str: args[2], data: args[3], res: args[4]
          });
        }
      }
      try {
        iface[method](...args, handler.bind(this));
      } catch (e) {
        reject(e);
      }
    });
  }
}


export default IpcDBus;
