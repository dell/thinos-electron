import { View } from 'electron';

const { TextView } = process._linkedBinding('electron_browser_text_view');

Object.setPrototypeOf(TextView.prototype, View.prototype);

export default TextView;
