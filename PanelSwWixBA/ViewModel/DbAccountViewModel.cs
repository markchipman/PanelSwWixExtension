﻿//-------------------------------------------------------------------------------------------------
// <copyright file="DbAccountViewModel.cs" company="Panel-SW.com">
//   Copyright (c) 2015, Panel-SW.com.
//   This software is released under Microsoft Reciprocal License (MS-RL).
//   The license and further copyright text can be found in the file
//   LICENSE.TXT at the root directory of the distribution.
// </copyright>
//-------------------------------------------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using Microsoft.Tools.WindowsInstallerXml.Bootstrapper;
using System.Windows.Input;
using PanelSW.WixBA.View;
using PanelSW.WixBA.Utils;
using System.DirectoryServices;
using System.DirectoryServices.ActiveDirectory;
using System.DirectoryServices.AccountManagement;
using System.Data.Sql;
using System.Data;
using System.Data.SqlClient;
using System.IO;
using System.Windows.Forms;
using System.Net.Sockets;

namespace PanelSW.WixBA
{
    /// <summary>
    /// Validate that the machine is part of a domain 
    /// </summary>
    public class DbAccountViewModel : ViewModelBase
    {
        private RootViewModel root;

        public DbAccountViewModel(RootViewModel root)
            : base(root)
        {
            this.root = root;
        }

        public override string Title
        {
            get
            {
                return "SQL Server Connection";
            }
        }

        public string SqlServer
        {
            get
            {
                return PanelSwWixBA.Model.SqlServer;
            }
            set
            {
                PanelSwWixBA.Model.SqlServer = value;
                OnPropertyChanged("SqlServer");
            }
        }

        public string DatabaseName
        {
            get
            {
                return PanelSwWixBA.Model.SqlDbName;
            }
            set
            {
                PanelSwWixBA.Model.SqlDbName = value;
                OnPropertyChanged("DatabaseName");
            }
        }

        public bool SqlAuthentication
        {
            get
            {
                return PanelSwWixBA.Model.SqlAuth;
            }
            set
            {
                PanelSwWixBA.Model.SqlAuth = value;
                OnPropertyChanged("SqlAuthentication");
            }
        }

        public string UserName
        {
            get
            {
                return PanelSwWixBA.Model.SqlUserName;
            }
            set
            {
                PanelSwWixBA.Model.SqlUserName = value;
                OnPropertyChanged("UserName");
            }
        }

        #region Test Connection String

        private ICommand _testConnectionCommand = null;
        public ICommand TestConnectionCommand
        {
            get
            {
                if( _testConnectionCommand == null)
                {
                    _testConnectionCommand = new RelayCommand(
                        (a) =>
                        {
                            if( TestConnectionString())
                            {
                                PanelSwWixBA.Dispatcher.Invoke((Action)delegate()
                                {
                                    MessageBox.Show(
                                        "SQL Connection String validation succeeded."
                                        , "SQL Connection Valid"
                                        , MessageBoxButtons.OK
                                        , MessageBoxIcon.Information
                                        );
                                }
                                    );
                            }
                            else
                            {
                                PanelSwWixBA.Dispatcher.Invoke((Action)delegate()
                                {
                                    MessageBox.Show(
                                        "SQL Connection String validation failed."
                                        , "SQL Connection Error"
                                        , MessageBoxButtons.OK
                                        , MessageBoxIcon.Error
                                        );
                                }
                                    );
                            }
                        }
                        );
                }

                return _testConnectionCommand;
            }
        }

        private bool TestConnectionString()
        {
            try
            {
                SqlConnectionStringBuilder csBuilder = new SqlConnectionStringBuilder();
                csBuilder.DataSource = SqlServer;
                csBuilder.InitialCatalog = DatabaseName;
                if (string.IsNullOrWhiteSpace(UserName))
                {
                    csBuilder.IntegratedSecurity = true;
                }
                else
                {
                    csBuilder.IntegratedSecurity = false;
                    csBuilder.UserID = UserName;
                    csBuilder.Password = ((DbAccountView)root.DbAccountView).DbAccountPassword;
                }

                using (SqlConnection conn = new SqlConnection(csBuilder.ConnectionString))
                {
                    conn.Open();
                }

                return true;
            }
            catch( Exception ex)
            {
                PanelSwWixBA.Model.Engine.Log(LogLevel.Error
                    , string.Format(
                    "Exception caugth on testing SQL connection string: {0}: {1}"
                    , ex.GetType().ToString()
                    , ex.Message
                    )
                    );

                return false;
            }
        }

        #endregion

        #region Next Button (Button 3)

        private ICommand _nextCommand;
        public override ICommand Button3Command
        {
            get
            {
                if (_nextCommand == null)
                {
                    _nextCommand = new RelayCommand(
                        (a) =>
                        {
                            PanelSwWixBA.Model.SqlPassword = ((DbAccountView)root.DbAccountView).DbAccountPassword;

                            PanelSwWixBA.Plan(LaunchAction.Install);
                            _root.CurrentView = _root.ProgressView;
                        },
                        (a) =>
                            true
                        );
                }

                return _nextCommand;
            }
        }

        public override object Button3Content
        {
            get
            {
                return "Install";
            }
        }

        public override System.Windows.Visibility Button3Visibility
        {
            get
            {
                return System.Windows.Visibility.Visible;
            }
        }

        #endregion

        #region Back Button (Button 2)

        private ICommand _backCommand;
        public override ICommand Button2Command
        {
            get
            {
                if (_backCommand == null)
                {
                    _backCommand = new RelayCommand(
                        (a) =>
                        {
                            root.CurrentView = root.InstallDirView;
                        },
                        (a) =>
                            true
                        );
                }

                return _backCommand;
            }
        }

        public override object Button2Content
        {
            get
            {
                return "Back";
            }
        }

        public override System.Windows.Visibility Button2Visibility
        {
            get
            {
                return System.Windows.Visibility.Visible;
            }
        }

        #endregion
    }

    public enum SqlAuthentication
    {
        Sql,
        Windows
    }
}