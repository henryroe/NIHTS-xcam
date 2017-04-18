import numpy as np
import ctypes
import xenics
from astropy.io import fits
import time
import os

# TODO: better ctrl-c handling to kill just current sequence
# TODO: include a github tag stamp whatever in each FITS header


import xmltodict
import stomp
from stomp.listener import ConnectionListener
import sys
import datetime as dt

default_host = 'joe.lowell.edu'
default_port = 61613


class tcsStatus_subscriber(ConnectionListener):
    def on_message(self, headers, body):
        xml = xmltodict.parse(body)
        getset_dct_status(tcsStatus=xml)
                                      
                                      
class tcsTelemetry_subscriber(ConnectionListener):
    def on_message(self, headers, body):
        xml = xmltodict.parse(body)
        getset_dct_status(tcsTelemetry=xml)
                                      
                                      
class aos_subscriber(ConnectionListener):
    def on_message(self, headers, body):
        xml = xmltodict.parse(body)
        getset_dct_status(aos=xml)


def getset_dct_status(tcsStatus=None, tcsTelemetry=None, aos=None, save_status=[None, None, None]):
    # is used for BOTH getting/setting TCS/AOS status
    if tcsStatus is not None:
        save_status[0] = tcsStatus
    if tcsTelemetry is not None:
        save_status[1] = tcsTelemetry
    if aos is not None:
        save_status[2] = aos
    tcsStatus_xml = save_status[0]
    tcsTelemetry_xml = save_status[1]
    aos_xml = save_status[2]
    if tcsStatus is None and tcsTelemetry is None and aos is None:
        return tcsStatus_xml, tcsTelemetry_xml, aos_xml


class XenicsCamera():
    def __init__(self):
        self._pwm = 3000  
        self._exptime_sec = 1.0
        self._gain = False  # not sure if True is what xenics calls low or high gain, but True -> deeper wells, more e- per ADU.
        self._fan = True
        if self._open_camera():
            self._coadds = 1
            self._nexp = 1
            self._target_name = "Default Object Name"
            self._max_height = xenics.get_max_height()
            self._max_width = xenics.get_max_width()
            self._cur_file_num = 1
            obsdatadir = os.path.expanduser('~/xenics-data/')
            if not os.path.isdir(obsdatadir):
                os.mkdir(obsdatadir)
            self.obsdatadir = obsdatadir
            self._last_datadir_used = self._get_current_datadir()
        self.tcsStatus_conn = stomp.Connection([(default_host, default_port)])
        self.tcsStatus_conn.set_listener('tcs-status-subscriber', tcsStatus_subscriber())
        self.tcsStatus_conn.start()
        self.tcsStatus_conn.connect()
        self.tcsStatus_conn.subscribe('/topic/TCS.TCSSharedVariables.TCSHighLevelStatusSV.TCSTcsStatusSV', 123)
        self.tcsTelemetry_conn = stomp.Connection([(default_host, default_port)])
        self.tcsTelemetry_conn.set_listener('tcs-telemetry-subscriber', tcsTelemetry_subscriber())
        self.tcsTelemetry_conn.start()
        self.tcsTelemetry_conn.connect()
        self.tcsTelemetry_conn.subscribe('/topic/tcs.loisTelemetry', 234)
        self.aos_conn = stomp.Connection([(default_host, default_port)])
        self.aos_conn.set_listener('aos-status-subscriber', aos_subscriber())
        self.aos_conn.start()
        self.aos_conn.connect()
        self.aos_conn.subscribe('/topic/AOS.AOSPubDataSV.AOSDataPacket', 345)

    def _get_current_datadir(self):
        today_str = dt.datetime.utcnow().date().isoformat()
        datadir = os.path.join(self.obsdatadir, today_str)
        if not os.path.isdir(datadir):
            os.mkdir(datadir)
        return datadir
    
    def _get_next_filename(self):
        datadir = self._get_current_datadir()
        if datadir != self._last_datadir_used:  # reset if UT date has changed
            self._cur_file_num = 1
        filename = os.path.join(datadir, 
                                "{0}_{1:04n}.fits".format(dt.datetime.utcnow().date().isoformat(), 
                                                          self._cur_file_num))
        while os.path.exists(filename):
            self._cur_file_num += 1
            filename = os.path.join(datadir, 
                                    "{0}_{1:04n}.fits".format(dt.datetime.utcnow().date().isoformat(), 
                                                              self._cur_file_num))
        self._last_datadir_used = datadir
        return filename

    def _open_camera(self):
        # TK: switch to powering on/off directly using the pwrusb command line tool
        raw_input("Press return to confirm you've power cycled the camera & the camera power is now ON.")
        self.serial_number = '3731'
        print("Assuming camera serial number is {}".format(self.serial_number))
        time.sleep(3)
        if xenics.open_camera() != 0:
            print("Camera was not opened successfully.  Is it plugged in to USB and powered on?")
            return False
        print("waiting 5 sec after opening camera for connection to stabilize")
        time.sleep(5)
        self.set_pwm(self._pwm)
        self.exptime(self._exptime_sec)
        self.set_gain(self._gain)
        self.set_fan(True)
        return True

    def close_camera(self):
        xenics.close_camera()
        time.sleep(1)
        # TK: switch to powering on/off directly using the pwrusb command line tool
        raw_input("Press return to confirm you've disconnected power from camera.")
        print("-----\nI recommend you now exit/close this python session and start a new session\n" + 
              "before launching the camera again\n-----")

    def set_pwm(self, new_pwm):
        """
        Set the power to the thermoelectric cooler.
        
        Allowable range is 0-4095 and input will be trimmed to this.
        
        It is not recommended to run the camera at max power for long periods of time.
        """
        new_pwm = min(max(0, new_pwm), 4095)
        xenics.set_pwm(new_pwm)
        self._pwm = new_pwm
    
    def set_fan(self, new_fan):
        """
        True/False turns on the cooling fan.
        
        Currently there is no reason to ever turn off the cooling fan.
        """
        xenics.set_fan(new_fan)
        self._fan = new_fan
        
    def set_gain(self, new_gain):
        """
        Set gain state (True/False):
        True - deeper wells w/ more electrons per ADU      (longer exposure times, but more quantization noise)
        False - shallower wells w/ fewer electons per ADU  (shorter exposure times)
        """
        xenics.set_gain(new_gain)
        self.set_fan(self._fan)  # added 2015-06-19 because changing gain was turning off fan
        self._gain = new_gain
    
    def get_pwm(self):
        return self._pwm

    def get_fan(self):
        return self._fan
        
    def get_gain(self):
        return self._gain
    
    def exptime(self, exptime_sec=None):
        """
        input is in seconds
        
        if no input is given, just returns the current exptime in seconds.
        """
        if exptime_sec is not None:
            xenics.set_integration_time_millisec(np.int(np.round(exptime_sec * 1000.)))
            self._exptime_sec = exptime_sec
        return self._exptime_sec
        
    def coadds(self, input=None):
        """
        Sets the number of coadds.
        
        If no input is given, just returns the current number of coadds.
        """
        if input is not None:
            self._coadds = input
        return self._coadds
    
    def nexp(self, input=None):
        """
        Number of exposures to take in next GO.
        
        Total exposure time will be:  nexp * coadds * exptime_sec
        
        input=-1 indicates continuous ("video" mode)
        """
        if input is not None:
            self._nexp = input
        return self._nexp
        
    def target(self, input=None):
        """
        Set name of target.  Will be saved to FITS header.
        """
        if input is not None:
            self._target_name = input
        return self._target_name
            
    def _get_ADU_temperature(self, nreads=5):
        adu = np.zeros([nreads])
        for i in np.arange(nreads):
            adu[i] = xenics.get_temperature_ADU()
        return np.median(adu)

    def _get_ADCtype_temperature(self, nreads=5):
        adc = np.zeros([nreads])
        for i in np.arange(nreads):
            adc[i] = xenics.get_temperature_ADCtype()
        return np.median(adc)

    def _convert_adu_to_kelvin(self, adu_value):
        offset_temperature = 55  # TODO: 2012-08-23: am not yet convinced this is right value,
                                 #                   though no strong evidence it's wrong
        return offset_temperature + (50. + ((1133. - (((((adu_value * 2500. ) / 65536.) +
                                                        2866.) * 10.) / 46.)) * (250.)) / (400.))

    def go(self, exptime_sec=None, coadds=None, nexp=None):
        """
        Take an exposure sequence.
        
        nexp=-1 indicates continuous ("video" mode)
                
        Total exposure time will be roughly:  nexp * coadds * exptime_sec
        
        Each frame is saved to disk to an automatically generated filename, e.g.:
            ~/xenics-data/YYYY-MM-DD/YYYY_MM-DD-NNNN.fits
        where YYYY-MM-DD is the UT date at the end of the exposure sequence
        """
        self.exptime(exptime_sec)
        self.coadds(coadds)
        self.nexp(nexp)
        # reset PWM start of each sequence; there's been occasional hints that camera can 'forget' its PWM setting
        self.set_pwm(self._pwm)
        single_exp_ims = np.zeros([self._coadds, self._max_height, self._max_width], dtype=ctypes.c_ushort)
        single_exp_ims_1d = single_exp_ims.view().reshape(-1)
        im = np.zeros([self._max_height, self._max_width], dtype=np.int32)
        cur_nexp = 1
        while (cur_nexp <= self._nexp) or (self._nexp == -1):
            tk_adu1 = self._get_ADU_temperature(nreads=5)
            tk_adc1 = self._get_ADCtype_temperature(nreads=5)
            start_datetime = dt.datetime.utcnow()
            start_date_str = start_datetime.isoformat()
            single_exp_ims_1d[:] = 0
            xenics.capture_frames(single_exp_ims_1d)
            # note that numpy automatically upgrades the type to uint64 when doing the sum
            im[:, :] = single_exp_ims.sum(axis=0)
            end_datetime = dt.datetime.utcnow()
            end_date_str = end_datetime.isoformat()
            print("exp of {} coadds of {} seconds: took {} sec, expected ~{} sec".format(
                   self.coadds(), self.exptime(), 
                   (end_datetime - start_datetime).total_seconds(), 
                   self.coadds()*self.exptime()))
            tk_adu2 = self._get_ADU_temperature(nreads=5)
            tk_adc2 = self._get_ADCtype_temperature(nreads=5)
            hdu = fits.PrimaryHDU(ims)
            hdulist = fits.HDUList([hdu])
            hdu.header['OBJECT'] = self._target_name
            hdu.header['DATE-OBS'] = dt.datetime.utcnow().isoformat()
            hdu.header['EXPTIME'] = (self._exptime_sec, "exposure time in seconds")
            hdu.header['COADDS'] = (self._coadds, "number of coadds per frame written to disk")
            hdu.header['NEXP'] = (self._nexp, "total number of frames in current sequence")
            hdu.header['DATE-BEG'] = (start_date_str, "UT date time at sequence start")
            hdu.header['DATE-END'] = (end_date_str, "UT date time at sequence end")
            hdu.header['FILENAME'] = "current.fits"
            hdu.header['INSTRUME'] = "Xenics serial number {}".format(self.serial_number)
            hdu.header['PWM'] = (self._pwm, "xenics cooling power setting")
            hdu.header['FAN'] = (self._fan, "xenics fan setting")
            hdu.header['TK_ADU1'] = (tk_adu1, 'get_temperature_ADU at sequence start')
            hdu.header['TK1'] = (self._convert_adu_to_kelvin(tk_adu1), 'T(K) at sequence start')
            hdu.header['TK_ADC1'] = (tk_adc1, 'get_temperature_ADCtype at sequence start')
            hdu.header['TK_ADU2'] = (tk_adu2, 'get_temperature_ADU at sequence end')
            hdu.header['TK2'] = (self._convert_adu_to_kelvin(tk_adu2), 'T(K) at sequence end')
            hdu.header['TK_ADC2'] = (tk_adc2, 'get_temperature_ADCtype at sequence end')
            tcsStatus,tcsTelemetry,aos = getset_dct_status()
            if tcsStatus is not None:
    #             print(tcs)
    #             print(aos)
    # 
    # TCS Packet:  (rotator fixed)
    # OrderedDict([(u'tcsTCSStatus', OrderedDict([(u'accessMode', u'Operator'), (u'azCurrentWrap', u'1'), (u'heartbeat', u'8098'), (u'inPositionIsTrue', u'true'), (u'm1CoverState', u'Open'), (u'mountGuideMode', u'OpenLoop'), (u'rotCurrentWrap', u'-1'), (u'tcsHealth', u'GOOD'), (u'tcsState', u'ENABLED'), (u'currentTimes', OrderedDict([(u'lst', OrderedDict([(u'hours', u'9'), (u'minutesTime', u'45'), (u'secondsTime', u'23')])), (u'time', u'2016-04-20T03:16:30.528+00:00')])), (u'limits', OrderedDict([(u'moonProximity', OrderedDict([(u'distance_deg', u'83.382785373422'), (u'proximityFlag', u'false')])), (u'sunProximity', OrderedDict([(u'distance_deg', u'76.564887381463'), (u'proximityFlag', u'false')])), (u'zenith', OrderedDict([(u'currentZD_deg', u'50.837381'), (u'elZenithLimit_deg', u'89.3'), (u'inBlindSpotIsTrue', u'false'), (u'timeToBlindSpot_min', u'-1'), (u'timeToBlindSpotExit_min', u'-1')])), (u'airmass', u'1.580980523677'), (u'currentTimeToObservable_min', u'-1'), (u'currentTimeToUnobservable_min', u'175'), (u'timeToRotLimit_min', u'-1'), (u'timeToAzLimit_min', u'925')])), (u'pointingPositions', OrderedDict([(u'azElError', OrderedDict([(u'azError', u'-0.0252'), (u'elError', u'0.0108')])), (u'currentAzEl', OrderedDict([(u'azimuth', OrderedDict([(u'degreesArc', u'238'), (u'minutesArc', u'5'), (u'secondsArc', u'48.7')])), (u'elevation', OrderedDict([(u'degreesAlt', u'39'), (u'minutesArc', u'9'), (u'secondsArc', u'45.4')]))])), (u'currentHA', OrderedDict([(u'hours', u'2'), (u'minutesTime', u'45'), (u'secondsTime', u'36.11')])), (u'currentRADec', OrderedDict([(u'declination', OrderedDict([(u'degreesDec', u'1'), (u'minutesArc', u'28'), (u'secondsArc', u'47')])), (u'equinoxPrefix', u'J'), (u'equinoxYear', u'2000'), (u'frame', u'FK5'), (u'ra', OrderedDict([(u'hours', u'6'), (u'minutesTime', u'58'), (u'secondsTime', u'54.88')]))])), (u'currentRotatorPositions', OrderedDict([(u'rotPA', u'175.059382'), (u'iaa', u'4.95'), (u'rotIPA', u'180')])), (u'demandAzEl', OrderedDict([(u'azimuth', OrderedDict([(u'degreesArc', u'238'), (u'minutesArc', u'5'), (u'secondsArc', u'49.93')])), (u'elevation', OrderedDict([(u'degreesAlt', u'39'), (u'minutesArc', u'9'), (u'secondsArc', u'45.07')]))])), (u'demandRADec', OrderedDict([(u'declination', OrderedDict([(u'degreesDec', u'1'), (u'minutesArc', u'28'), (u'secondsArc', u'47.4')])), (u'equinoxPrefix', u'J'), (u'equinoxYear', u'2000'), (u'frame', u'FK5'), (u'ra', OrderedDict([(u'hours', u'6'), (u'minutesTime', u'58'), (u'secondsTime', u'54.82')]))])), (u'demandRotatorPositions', OrderedDict([(u'rotPA', u'175.059372')])), (u'targetName', u'0914-0119451'), (u'currentParAngle', u'44.569672815492')])), (u'axesTrackMode', u'All'), (u'inPositionAzIsTrue', u'true'), (u'inPositionElIsTrue', u'true'), (u'inPositionRotIsTrue', u'true'), (u'externalTargetCfgCmdPreviewIsTrue', u'false')]))])
                hdu.header['TELESCOP'] = ('DCT', 'Telescope name')
                hdu.header['TCS-TIME'] = (tcsStatus['tcsTCSStatus']['currentTimes']['time'], 'TCS Packet UTC time')
                hdu.header['TCS-OK'] = (tcsStatus['tcsTCSStatus']['inPositionIsTrue'] == 'true', 'TCS inPositionIsTrue')
                rah = np.int(tcsStatus['tcsTCSStatus']['pointingPositions']['currentRADec']['ra']['hours'])
                ram = np.int(tcsStatus['tcsTCSStatus']['pointingPositions']['currentRADec']['ra']['minutesTime'])
                ras = np.float(tcsStatus['tcsTCSStatus']['pointingPositions']['currentRADec']['ra']['secondsTime'])
                decd = np.int(tcsStatus['tcsTCSStatus']['pointingPositions']['currentRADec']['declination']['degreesDec'])
                decm = np.int(tcsStatus['tcsTCSStatus']['pointingPositions']['currentRADec']['declination']['minutesArc'])
                decs = np.float(tcsStatus['tcsTCSStatus']['pointingPositions']['currentRADec']['declination']['secondsArc'])
                hdu.header['RA'] = ('{0:2n}:{1:02n}:{2:05.2f}'.format(rah, ram, ras), 'RA HH:MM:SS.SS')
                hdu.header['DEC'] = ('{0:+3n}:{1:02n}:{2:04.1f}'.format(decd, decm, decs), 'DEC DDD:MM:SS.SS')
                az = (np.float(tcsStatus['tcsTCSStatus']['pointingPositions']['currentAzEl']['azimuth']['degreesArc']) +
                      np.float(tcsStatus['tcsTCSStatus']['pointingPositions']['currentAzEl']['azimuth']['minutesArc'])/60. +
                      np.float(tcsStatus['tcsTCSStatus']['pointingPositions']['currentAzEl']['azimuth']['secondsArc'])/3600.)
                el = (np.float(tcsStatus['tcsTCSStatus']['pointingPositions']['currentAzEl']['elevation']['degreesAlt']) +
                      np.float(tcsStatus['tcsTCSStatus']['pointingPositions']['currentAzEl']['elevation']['minutesArc'])/60. +
                      np.float(tcsStatus['tcsTCSStatus']['pointingPositions']['currentAzEl']['elevation']['secondsArc'])/3600.)
                hdu.header['AZ'] = (az, 'Azimuth (deg)')
                hdu.header['EL'] = (el, 'Elevation (deg)')
                hdu.header['AIRMASS'] = (np.float(tcsStatus['tcsTCSStatus']['limits']['airmass']), 'Airmass')
                hdu.header['TARGNAME'] = (tcsStatus['tcsTCSStatus']['pointingPositions']['targetName'], 'TCS Target Name')
                hdu.header['LST'] = ('{0:2n}:{1:02n}:{2:04.1f}'.format(
                                          int(tcsStatus['tcsTCSStatus']['currentTimes']['lst']['hours']),
                                          int(tcsStatus['tcsTCSStatus']['currentTimes']['lst']['minutesTime']),
                                          float(tcsStatus['tcsTCSStatus']['currentTimes']['lst']['secondsTime'])),
                                     'TCS LST')
                hdu.header['rotIPA'] = (np.float(tcsStatus['tcsTCSStatus']['pointingPositions']['currentRotatorPositions']['rotIPA']),
                                        'TCS rotIPA')
            if tcsTelemetry is not None:
                # TODO: clean up headers coming from TCS Telemetry, right now am just trying to mirror the xml packet grammar as closely as possible
                hdu.header['TCSLST'] = (tcsTelemetry['TCSTelemetry']['TCSLST'], 'TCSTelemetry TCSLST')
                hdu.header['DEMANDRA'] = (tcsTelemetry['TCSTelemetry']['DemandRa'], 'TCSTelemetry DemandRa')
                hdu.header['DEMANDDE'] = (tcsTelemetry['TCSTelemetry']['DemandDec'], 'TCSTelemetry DemandDec')
                try:
                    hdu.header['TCSCURAZ'] = (np.float(tcsTelemetry['TCSTelemetry']['TCSCurrentAzimuth']), 'TCSTelemetry TCSCurrentAzimuth')
                except ValueError:
                    hdu.header['TCSCURAZ'] = (tcsTelemetry['TCSTelemetry']['TCSCurrentAzimuth'], 'TCSTelemetry TCSCurrentAzimuth')      
                try:
                    hdu.header['TCSCUREL'] = (np.float(tcsTelemetry['TCSTelemetry']['TCSCurrentElev']), 'TCSTelemetry TCSCurrentElev')
                except ValueError:
                    hdu.header['TCSCUREL'] = (tcsTelemetry['TCSTelemetry']['TCSCurrentElev'], 'TCSTelemetry TCSCurrentElev')
                hdu.header['MNTGMODE'] = (tcsTelemetry['TCSTelemetry']['MountGuideMode'], 'TCSTelemetry MountGuideMode')
                hdu.header['SCITARGN'] = (tcsTelemetry['TCSTelemetry']['ScienceTargetName'], 'TCSTelemetry ScienceTargetName')
                hdu.header['M1COVER'] = (tcsTelemetry['TCSTelemetry']['m1CoverState'], 'TCSTelemetry m1CoverState')
                try:
                    hdu.header['DOMEDAZ'] = (np.float(tcsTelemetry['TCSTelemetry']['MountDomeAzimuthDifference']), 'TCSTelemetry MountDomeAzimuthDifference')
                except ValueError:
                    hdu.header['DOMEDAZ'] = (tcsTelemetry['TCSTelemetry']['MountDomeAzimuthDifference'], 'TCSTelemetry MountDomeAzimuthDifference')
                hdu.header['DOMEWARN'] = (tcsTelemetry['TCSTelemetry']['DomeOccultationWarning'], 'TCSTelemetry DomeOccultationWarning')
                try:
                    hdu.header['PARANGLE'] = (np.float(tcsTelemetry['TCSTelemetry']['CurrentParAngle']), 'TCSTelemetry CurrentParAngle')
                except ValueError:
                    hdu.header['PARANGLE'] = (tcsTelemetry['TCSTelemetry']['CurrentParAngle'], 'TCSTelemetry CurrentParAngle')
                try:
                    hdu.header['TCSroPA'] = (np.float(tcsTelemetry['TCSTelemetry']['TCSCurrentRotatorPA']), 'TCSTelemetry TCSCurrentRotatorPA')
                except ValueError:
                    hdu.header['TCSroPA'] = (tcsTelemetry['TCSTelemetry']['TCSCurrentRotatorPA'], 'TCSTelemetry TCSCurrentRotatorPA')
                try:
                    hdu.header['TCSroIAA'] = (np.float(tcsTelemetry['TCSTelemetry']['TCSCurrentRotatorIAA']), 'TCSTelemetry TCSCurrentRotatorIAA')
                except ValueError:
                    hdu.header['TCSroIAA'] = (tcsTelemetry['TCSTelemetry']['TCSCurrentRotatorIAA'], 'TCSTelemetry TCSCurrentRotatorIAA')
                try:
                    hdu.header['TCSroIPA'] = (np.float(tcsTelemetry['TCSTelemetry']['TCSCurrentRotatorIPA']), 'TCSTelemetry TCSCurrentRotatorIPA')
                except ValueError:
                    hdu.header['TCSroIPA'] = (tcsTelemetry['TCSTelemetry']['TCSCurrentRotatorIPA'], 'TCSTelemetry TCSCurrentRotatorIPA')
                hdu.header['ROTFRAME'] = (tcsTelemetry['TCSTelemetry']['RotatorFrame'], 'TCSTelemetry RotatorFrame')
                hdu.header['TARGFRAM'] = (tcsTelemetry['TCSTelemetry']['TargetFrame'], 'TCSTelemetry TargetFrame')
                try:
                    hdu.header['TCSEQUIN'] = (np.float(tcsTelemetry['TCSTelemetry']['equinox']), 'TCSTelemetry equinox')
                except ValueError:
                    hdu.header['TCSEQUIN'] = (tcsTelemetry['TCSTelemetry']['equinox'], 'TCSTelemetry equinox')
                hdu.header['TCSState'] = (tcsTelemetry['TCSTelemetry']['TCSState'], 'TCSTelemetry TCSState')
                hdu.header['TCSHealt'] = (tcsTelemetry['TCSTelemetry']['TCSHealth'], 'TCSTelemetry TCSHealth')
                hdu.header['TCSAMODE'] = (tcsTelemetry['TCSTelemetry']['TCSAccessMode'], 'TCSTelemetry TCSAccessMode')
                hdu.header['TCSINPOS'] = (tcsTelemetry['TCSTelemetry']['InPosition'], 'TCSTelemetry InPosition')
                try:
                    hdu.header['TCSMNTTC'] = (np.float(tcsTelemetry['TCSTelemetry']['MountTemperature']), 'TCSTelemetry MountTemperature')
                except ValueError:
                    hdu.header['TCSMNTTC'] = (tcsTelemetry['TCSTelemetry']['MountTemperature'], 'TCSTelemetry MountTemperature')
                hdu.header['CLSLOBAN'] = (tcsTelemetry['TCSTelemetry']['CLSLowBankState'], 'TCSTelemetry CLSLowBankState')
                hdu.header['DSSPOSST'] = (tcsTelemetry['TCSTelemetry']['DSSPositionStatus'], 'TCSTelemetry DSSPositionStatus')
                # TODO: consider making a test for age of TCSTelemetry packet age and changing OK status accordingly
                hdu.header['TCSTELEM'] = (True, 'TCSTelemetry packet OK')
            else:
                hdu.header['TCSTELEM'] = (False, 'TCSTelemetry packet not OK')
    # AOS Packet:
    # OrderedDict([(u'AOSDataPacket', OrderedDict([(u'timestamp', u'2016-04-20T03:16:29.988+00:00'), (u'detailedState', u'UnlockedOpenLoopState'), (u'summaryState', u'Enabled'), (u'tipTiltPistonDemandM1', OrderedDict([(u'X_Tilt_rad', u'0'), (u'Y_Tilt_rad', u'0'), (u'Piston_m', u'0')])), (u'tipTiltPistonDemandM2', OrderedDict([(u'X_Tilt_rad', u'-0.0001380827127709'), (u'Y_Tilt_rad', u'0.00015234633023942'), (u'Piston_m', u'0.00011020825009747')])), (u'comaPointingOffset', OrderedDict([(u'xCorrection_arcsec', u'-18.538316440227'), (u'yCorrection_arcsec', u'-16.802643163435')])), (u'totalFocusOffset', u'0.0006'), (u'focusOffsetDemandOutOfRange', u'false'), (u'wavefrontDataOutOfRange', u'false'), (u'M1FSettled', u'true'), (u'M1LSettled', u'true'), (u'M1PSettled', u'true'), (u'M2PSettled', u'true'), (u'M2VSettled', u'true')]))])

            if aos is not None:
                hdu.header['FOCUS'] = (1e6*np.float(aos['AOSDataPacket']['totalFocusOffset']), 'Focus (in microns)')
                hdu.header['AOSTIME'] = (aos['AOSDataPacket']['timestamp'], 'AOS timestamp')
                hdu.header['AOSDETAI'] = (aos['AOSDataPacket']['detailedState'], 'AOS detailedState')
                hdu.header['AOSSUMMA'] = (aos['AOSDataPacket']['summaryState'], 'AOS summaryState')
                # have not bothered to inlude tiptilt pistons, coma pointing offsets
                hdu.header['M1FSettl'] = (aos['AOSDataPacket']['M1FSettled'] == 'true', 'AOS M1FSettled')
                hdu.header['M1LSettl'] = (aos['AOSDataPacket']['M1LSettled'] == 'true', 'AOS M1LSettled')
                hdu.header['M1PSettl'] = (aos['AOSDataPacket']['M1PSettled'] == 'true', 'AOS M1PSettled')
                hdu.header['M2PSettl'] = (aos['AOSDataPacket']['M2PSettled'] == 'true', 'AOS M2PSettled')
                hdu.header['M2VSettl'] = (aos['AOSDataPacket']['M2VSettled'] == 'true', 'AOS M2VSettled')
                hdu.header['FOCUS_OK'] = (aos['AOSDataPacket']['focusOffsetDemandOutOfRange'] == 'false', 
                                          'AOS focusOffsetDemandOutOfRange == false')
                hdu.header['WAVEF_OK'] = (aos['AOSDataPacket']['wavefrontDataOutOfRange'] == 'false', 
                                          'AOS wavefrontDataOutOfRange == false')

            filename = self._get_next_filename()
            hdu.header['FILENAME'] = (os.path.basename(filename), 'original filename as written to disk')
            hdulist.writeto(filename)
            print("wrote {} to disk".format(os.path.basename(filename)))
    
        
        
