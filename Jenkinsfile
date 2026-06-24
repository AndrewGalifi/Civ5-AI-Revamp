pipeline {
    agent {
        label 'windows && civ5'
    }

    options {
        disableConcurrentBuilds()
        buildDiscarder(logRotator(numToKeepStr: '10'))
    }

    stages {
        stage('Checkout') {
            steps {
                checkout scm
            }
        }

        stage('Build DLL') {
            steps {
                powershell '''
                    $ErrorActionPreference = "Stop"
                    .\\scripts\\civ5_dll.ps1
                '''
            }
        }

        stage('Verify DLL') {
            steps {
                powershell '''
                    $dll = "CvGameCoreSource\\BuildOutput\\VS2013_ModWin32\\CvGameCoreDLL_Expansion2Win32Mod.dll"

                    if (-not (Test-Path -LiteralPath $dll)) {
                        throw "DLL not found: $dll"
                    }

                    Get-Item -LiteralPath $dll | Format-List FullName, Length, LastWriteTime
                '''
            }
        }

        stage('Archive DLL') {
            steps {
                archiveArtifacts(
                    artifacts: 'CvGameCoreSource/BuildOutput/VS2013_ModWin32/CvGameCoreDLL_Expansion2Win32Mod.dll',
                    fingerprint: true,
                    onlyIfSuccessful: true
                )
            }
        }
    }

    post {
        always {
            deleteDir()
        }
    }
}
