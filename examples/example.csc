<?xml version="1.0" encoding="UTF-8"?>
<simconf>
  <project EXPORT="discard">[APPS_DIR]/mrm</project>
  <project EXPORT="discard">[APPS_DIR]/mspsim</project>
  <project EXPORT="discard">[APPS_DIR]/avrora</project>
  <project EXPORT="discard">[APPS_DIR]/serial_socket</project>
  <project EXPORT="discard">[APPS_DIR]/collect-view</project>
  <project EXPORT="discard">[APPS_DIR]/powertracker</project>
  <project EXPORT="discard">[CONFIG_DIR]/../../../Contiki/eclipse/realsim_rep/cooja-plugin</project>
  <project EXPORT="discard">[CONFIG_DIR]/../../../Contiki/eclipse/gdbstub</project>
  <simulation>
    <title>My simulation</title>
    <randomseed>123456</randomseed>
    <motedelay_us>1000000</motedelay_us>
    <radiomedium>org.contikios.cooja.radiomediums.SilentRadioMedium</radiomedium>
    <events>
      <logoutput>40000</logoutput>
    </events>
    <motetype>
      org.contikios.cooja.mspmote.SkyMoteType
      <identifier>sky1</identifier>
      <description>Loadermote</description>
      <source EXPORT="discard">[CONFIG_DIR]/link.c</source>
      <commands EXPORT="discard">make all TARGET=sky</commands>
      <firmware EXPORT="copy">[CONFIG_DIR]/link.sky</firmware>
      <moteinterface>org.contikios.cooja.interfaces.Position</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.MoteAttributes</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspClock</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspMoteID</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.SkyButton</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.SkyFlash</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.SkyCoffeeFilesystem</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspSerial</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.SkyLED</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspDebugOutput</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.SkyTemperature</moteinterface>
    </motetype>
    <mote>
      <breakpoints />
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>2.9156728332245363</x>
        <y>70.42716656873628</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>1</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
  </simulation>
  <plugin>
    org.contikios.cooja.plugins.SimControl
    <width>280</width>
    <z>1</z>
    <height>160</height>
    <location_x>400</location_x>
    <location_y>0</location_y>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.Visualizer
    <plugin_config>
      <moterelations>true</moterelations>
      <skin>org.contikios.cooja.plugins.skins.IDVisualizerSkin</skin>
      <skin>org.contikios.cooja.plugins.skins.GridVisualizerSkin</skin>
      <skin>org.contikios.cooja.plugins.skins.TrafficVisualizerSkin</skin>
      <viewport>0.9090909090909091 0.0 0.0 0.9090909090909091 191.34938833343224 108.97530311933066</viewport>
    </plugin_config>
    <width>400</width>
    <z>3</z>
    <height>400</height>
    <location_x>1</location_x>
    <location_y>1</location_y>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.LogListener
    <plugin_config>
      <filter />
      <formatted_time />
      <coloring />
    </plugin_config>
    <width>1960</width>
    <z>4</z>
    <height>240</height>
    <location_x>400</location_x>
    <location_y>160</location_y>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.Notes
    <plugin_config>
      <notes>Enter notes here</notes>
      <decorations>true</decorations>
    </plugin_config>
    <width>1680</width>
    <z>5</z>
    <height>160</height>
    <location_x>680</location_x>
    <location_y>0</location_y>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.ScriptRunner
    <plugin_config>
      <script>//TIMEOUT(100000); /* milliseconds. no action at timeout */&#xD;
TIMEOUT(100000, log.log("last msg: " + msg + "\n")); /* milliseconds. print last msg at timeout */&#xD;
&#xD;
//only one mote&#xD;
fs = mote.getFilesystem();&#xD;
dir = mote.getType().getContikiFirmwareFile().getParentFile().getAbsolutePath() + java.io.File.separator;&#xD;
log.log(dir);&#xD;
if(!fs.insertFile(dir + "hello.mlk")) log.testFailed();&#xD;
if(!fs.insertFile(dir + "sym.mls")) log.testFailed();&#xD;
&#xD;
log.log("waiting for hello world output from mote 1\n");&#xD;
WAIT_UNTIL(id == 1 &amp;&amp; msg.equals("Hello, world"));&#xD;
&#xD;
log.testOK();</script>
      <active>true</active>
    </plugin_config>
    <width>1548</width>
    <z>2</z>
    <height>833</height>
    <location_x>604</location_x>
    <location_y>383</location_y>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.MoteInterfaceViewer
    <mote_arg>0</mote_arg>
    <plugin_config>
      <interface>Coffee Filesystem</interface>
      <scrollpos>0,0</scrollpos>
    </plugin_config>
    <width>350</width>
    <z>0</z>
    <height>300</height>
    <location_x>1236</location_x>
    <location_y>159</location_y>
  </plugin>
</simconf>

