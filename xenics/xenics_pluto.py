import numpy as np
import ctypes
import xenics
import datetime
from astropy.io import fits
import time
import os

# TODO: better ctrl-c handling to kill just current sequence
# TODO: include a github tag stamp whatever in each FITS header

class XenicsCamera():
    def __init__(self):
        self._pwm = 3000  
        self._exptime_sec = 1.0
        self._gain = False  # not sure if True is what xenics calls low or high gain, but True -> deeper wells, more e- per ADU.
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
        
    def _get_current_datadir(self):
        today_str = datetime.datetime.utcnow().date().isoformat()
        datadir = os.path.join(self.obsdatadir, today_str)
        if not os.path.isdir(datadir):
            os.mkdir(datadir)
        return datadir
    
    def _get_next_filename(self):
        datadir = self._get_current_datadir()
        if datadir != self._last_datadir_used:  # reset if UT date has changed
            self._cur_file_num = 1
        filename = os.path.join(datadir, 
                                "{0}_{1:04n}.fits".format(datetime.datetime.utcnow().date().isoformat(), 
                                                          self._cur_file_num))
        while os.path.exists(filename):
            self._cur_file_num += 1
            filename = os.path.join(datadir, 
                                    "{0}_{1:04n}.fits".format(datetime.datetime.utcnow().date().isoformat(), 
                                                              self._cur_file_num))
        self._last_datadir_used = datadir
        return filename

    def _open_camera(self):
        raw_input("Press return to confirm you've power cycled the camera & the camera power is now ON.")
        self.serial_number = '1567'
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

    def go(self, exptime_sec=None, coadds=None, nexp=None, return_images=False,
           save_every_Nth_to_currentfits=None):
        """
        Take an exposure sequence.
                
        Total exposure time will be roughly:  nexp * coadds * exptime_sec
        
        The entire sequence is saved to disk at the end of the sequence to an automatically generated filename.
        
        To access images during sequence:
        save_every_Nth_to_currentfits = None -> don't save any images to disk until the entire sequence is saved
                                        1 -> save every exposure to disk as "current.fits" as it is acquired
                                        N -> save every Nth exposure to disk as "current.fits"
        Note that at shorter exposure times it is wise to set N to a high enough number that you are only 
        writing to disk every few seconds.
        
        return_images:  If True, then numpy array of images will be returned by this method (as well as saved to disk)
        """
        # TODO: play with TK conversion - unsure if working correctly.
        self.exptime(exptime_sec)
        self.coadds(coadds)
        self.nexp(nexp)
        # reset PWM start of each sequence; there's been occasional hints that camera can 'forget' its PWM setting
        self.set_pwm(self._pwm)
        self.set_fan(self._fan)
        self.set_gain(self._gain)
        single_exp_ims = np.zeros([self._coadds, self._max_height, self._max_width], dtype=ctypes.c_ushort)
        single_exp_ims_1d = single_exp_ims.view().reshape(-1)
        ims = np.zeros([self._nexp, self._max_height, self._max_width], dtype=np.int32)
        tk_adu1 = self._get_ADU_temperature(nreads=5)
        tk_adc1 = self._get_ADCtype_temperature(nreads=5)
        start_datetime = datetime.datetime.utcnow()
        start_date_str = start_datetime.isoformat()
        for cur_nexp in np.arange(self._nexp):
            single_exp_ims_1d[:] = 0
            xenics.capture_frames(single_exp_ims_1d)
            # note that numpy automatically upgrades the type to uint64 when doing the sum
            ims[cur_nexp, :, :] = single_exp_ims.sum(axis=0)
            if ((save_every_Nth_to_currentfits is not None) and 
                ((cur_nexp % save_every_Nth_to_currentfits) == 0)):
                print(cur_nexp)
                hdu = fits.PrimaryHDU(ims[cur_nexp, :, :])
                hdu.header['OBJECT'] = self._target_name
                hdu.header['DATE-OBS'] = datetime.datetime.utcnow().isoformat()
                hdu.header['EXPTIME'] = (self._exptime_sec, "exposure time in seconds")
                hdu.header['COADDS'] = (self._coadds, "number of coadds per frame written to disk")
                hdu.header['NEXP'] = (self._nexp, "total number of frames in current sequence")
                hdu.header['CUREXP_N']= (cur_nexp, "frame number in sequence, starting from 0")
                hdu.header['FILENAME'] = "current.fits"
                hdu.header['INSTRUME'] = "Xenics serial number {}".format(self.serial_number)
                hdu.header['PWM'] = (self._pwm, "xenics cooling power setting")
                hdu.header['FAN'] = (self._fan, "xenics fan setting")
                hdulist = fits.HDUList([hdu])
                hdulist.writeto(os.path.join(self._get_current_datadir(), 'current.fits'), clobber=True)
                print("Wrote {} of {} to current.fits".format(cur_nexp + 1, self._nexp))
        end_datetime = datetime.datetime.utcnow()
        end_date_str = end_datetime.isoformat()
        print("{} exp of {} coadds of {} seconds: took {} sec, expected {} sec".format(
               self.nexp(), self.coadds(), self.exptime(), 
               (end_datetime - start_datetime).total_seconds(), 
               self.nexp()*self.coadds()*self.exptime()))
        tk_adu2 = self._get_ADU_temperature(nreads=5)
        tk_adc2 = self._get_ADCtype_temperature(nreads=5)
        hdu = fits.PrimaryHDU(ims)
        hdulist = fits.HDUList([hdu])
        hdu.header['OBJECT'] = self._target_name
        hdu.header['DATE-OBS'] = datetime.datetime.utcnow().isoformat()
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
        filename = self._get_next_filename()
        hdu.header['FILENAME'] = (os.path.basename(filename), 'original filename as written to disk')
        hdulist.writeto(filename)
        print("wrote {} to disk".format(os.path.basename(filename)))
        if return_images:
            return ims
    
        
        
